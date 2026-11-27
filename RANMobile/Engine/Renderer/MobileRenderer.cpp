#include "MobileRenderer.h"
#include "../Camera/MobileCamera.h"
#include "../Core/Logger.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <fstream>
#include <cstring>

#ifdef __ANDROID__
#include <GLES3/gl3.h>
#endif

namespace RANMobile {

// B18: per-uploaded-mesh GL state (also carries the counts DrawGpuSkinnedMesh
// validates against, so headless builds exercise the same validation path).
struct GpuMeshGlState {
    uint32_t vao = 0;
    uint32_t vbo = 0;
    uint32_t ebo = 0;
    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
};

static constexpr int kAtlasSize = 1024;
static constexpr float kBaseFontPx = 28.0f;

static stbtt_fontinfo g_primaryFontInfo;
static stbtt_fontinfo g_thaiFontInfo;

#ifdef __ANDROID__
static const char* kUIVertexShader = R"(#version 300 es
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
uniform mat4 uProj;
out vec2 vUV;
void main() {
    gl_Position = uProj * vec4(aPos, 0.0, 1.0);
    vUV = aUV;
}
)";

static const char* kUIFragmentShader = R"(#version 300 es
precision mediump float;
uniform vec4 uColor;
uniform int uUseTexture;
uniform sampler2D uTex;
in vec2 vUV;
out vec4 FragColor;
void main() {
    if (uUseTexture == 1) {
        float alpha = texture(uTex, vUV).r;
        FragColor = vec4(uColor.rgb, uColor.a * alpha);
    } else if (uUseTexture == 2) {
        FragColor = texture(uTex, vUV) * uColor;
    } else {
        FragColor = uColor;
    }
}
)";

static GLuint CompileShader(GLenum type, const char* src) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        Logger::Error("MobileRenderer", std::string("Shader compile error: ") + log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

// ---------------------------------------------------------------------------
// B17: 3D character pass shaders.  The CPU pose path (EP7CharacterScene ->
// DxSkinMesh9_CPU-equivalent skinning) already outputs world-space positions and
// normals, so the vertex stage only applies the camera view-projection — the same
// D3DTS_WORLD=Identity convention DxSkinMesh9_CPU uses (its skinned vertices are
// produced in world space and the world transform is identity).
// ---------------------------------------------------------------------------
static const char* kCharVertexShader = R"(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
uniform mat4 uVP;
out vec3 vNormal;
out vec2 vUV;
void main() {
    gl_Position = uVP * vec4(aPos, 1.0);
    vNormal = aNormal;
    vUV = aUV;
}
)";

static const char* kCharFragmentShader = R"(#version 300 es
precision mediump float;
uniform vec4 uColor;
uniform int uUseTexture;
uniform sampler2D uTex;
in vec3 vNormal;
in vec2 vUV;
out vec4 FragColor;
void main() {
    // Authentic fixed key light: DxSkinMesh9_HLSL.cpp:179-181 sets
    // lhtDir = normalize(vec4(0,1,-1,0)) for every skinned draw (B18 aligns the
    // B17 placeholder light with the authentic value; ambient floor unchanged).
    vec3 lightDir = normalize(vec3(0.0, 1.0, -1.0));
    float ndl = max(dot(normalize(vNormal), lightDir), 0.0);
    vec3 lit = uColor.rgb * (0.35 + 0.65 * ndl);
    if (uUseTexture == 1) {
        lit *= texture(uTex, vUV).rgb;
    }
    FragColor = vec4(lit, uColor.a);
}
)";

// ---------------------------------------------------------------------------
// B18: GPU skinning program (authentic D3DINDEXEDHLSLVS convention).
//
// Vertex streams = the authentic HLSL declaration (DxSkinMesh9_HLSL.cpp:95-106:
// POSITION/NORMAL/TEXCOORD/TANGENT + BLENDWEIGHT FLOAT4 + BLENDINDICES FLOAT4;
// the tangent stream is not consumed yet — effects that need it are out of scope).
// Palette = std140 uniform block fed per draw from the shared UBO, entries =
// offsetMatrix * boneCombined (DxSkinMesh9_HLSL.cpp:220-229 "mWorldMatrixArray"
// parity; BuildBonePalette already computes exactly this).
//
// Row-major storage in a column-major mat4 preserves the original v*M row-vector
// semantics: GLSL M_glsl[c][r] = m[c][r] makes M_glsl*v == v*m (verified against
// TransformCoord/TransformNormal in EP7Math.h).
//
// The shader reproduces SkinVerticesCPU exactly: per-influence position transform
// with w-divide (TransformCoord), translation-dropped normals (TransformNormal via
// mat3), |sum-1|>1e-4 weight renormalization, zero-weight-sum -> bind passthrough,
// final normal normalization.
// ---------------------------------------------------------------------------
static const char* kCharGpuVertexShader = R"(#version 300 es
precision highp float;
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec4 aBoneIdx;
layout(location = 4) in vec4 aBoneWt;
layout(std140) uniform BonePaletteBlock {
    mat4 uBonePalette[256];
};
uniform mat4 uVP;
out vec3 vNormal;
out vec2 vUV;
void main() {
    vec4 acc = vec4(0.0);
    vec3 nAcc = vec3(0.0);
    float wSum = 0.0;
    for (int i = 0; i < 4; ++i) {
        float w = aBoneWt[i];
        if (w <= 0.0) continue;
        int bi = int(aBoneIdx[i] + 0.5);
        mat4 m = uBonePalette[bi];
        vec4 tp = m * vec4(aPos, 1.0);
        tp.xyz /= tp.w;                       // TransformCoord parity (affine: w==1)
        acc += w * tp;
        nAcc += w * (mat3(m) * aNormal);      // TransformNormal parity
        wSum += w;
    }
    vec3 pos;
    vec3 nrm;
    if (wSum <= 1e-6) {
        pos = aPos;                           // empty influence list: bind passthrough
        nrm = aNormal;
    } else {
        if (abs(wSum - 1.0) > 1e-4) {
            acc.xyz /= wSum;                  // SkinVerticesCPU renormalization parity
            nAcc /= wSum;
        }
        pos = acc.xyz;
        nrm = normalize(nAcc);
    }
    gl_Position = uVP * vec4(pos, 1.0);
    vNormal = nrm;
    vUV = aUV;
}
)";

static const char* kCharGpuFragmentShader = R"(#version 300 es
precision mediump float;
uniform vec4 uColor;
uniform int uUseTexture;
uniform sampler2D uTex;
in vec3 vNormal;
in vec2 vUV;
out vec4 FragColor;
void main() {
    vec3 lightDir = normalize(vec3(0.0, 1.0, -1.0));  // authentic lhtDir
    float ndl = max(dot(normalize(vNormal), lightDir), 0.0);
    vec3 lit = uColor.rgb * (0.35 + 0.65 * ndl);
    if (uUseTexture == 1) {
        lit *= texture(uTex, vUV).rgb;
    }
    FragColor = vec4(lit, uColor.a);
}
)";
#endif

uint32_t MobileRenderer::DecodeUTF8(const std::string& text, size_t& index) {
    if (index >= text.size()) return 0;
    uint8_t c = static_cast<uint8_t>(text[index]);
    if (c < 0x80) {
        index += 1;
        return c;
    } else if ((c & 0xE0) == 0xC0 && index + 1 < text.size()) {
        uint32_t cp = ((c & 0x1F) << 6) | (static_cast<uint8_t>(text[index + 1]) & 0x3F);
        index += 2;
        return cp;
    } else if ((c & 0xF0) == 0xE0 && index + 2 < text.size()) {
        uint32_t cp = ((c & 0x0F) << 12) |
                      ((static_cast<uint8_t>(text[index + 1]) & 0x3F) << 6) |
                      (static_cast<uint8_t>(text[index + 2]) & 0x3F);
        index += 3;
        return cp;
    } else if ((c & 0xF8) == 0xF0 && index + 3 < text.size()) {
        uint32_t cp = ((c & 0x07) << 18) |
                      ((static_cast<uint8_t>(text[index + 1]) & 0x3F) << 12) |
                      ((static_cast<uint8_t>(text[index + 2]) & 0x3F) << 6) |
                      (static_cast<uint8_t>(text[index + 3]) & 0x3F);
        index += 4;
        return cp;
    }
    index += 1;
    return 0xFFFD;
}

bool MobileRenderer::LoadFontFile(const std::string& path, std::vector<uint8_t>& outBuffer) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return false;
    std::streamsize sz = f.tellg();
    if (sz <= 0) return false;
    f.seekg(0, std::ios::beg);
    outBuffer.resize(static_cast<size_t>(sz));
    return f.read(reinterpret_cast<char*>(outBuffer.data()), sz).good();
}

bool MobileRenderer::Initialize(int width, int height) {
    m_width = width > 0 ? width : 1280;
    m_height = height > 0 ? height : 720;
    m_settings.ApplyPreset(GraphicsQuality::Medium);

    const char* primaryPaths[] = {
        "/system/fonts/Roboto-Regular.ttf",
        "/system/fonts/NotoSans-Regular.ttf",
        "/system/fonts/DroidSans.ttf"
    };
    for (const char* p : primaryPaths) {
        if (LoadFontFile(p, m_primaryFontData) &&
            stbtt_InitFont(&g_primaryFontInfo, m_primaryFontData.data(), 0)) {
            m_hasPrimaryFont = true;
            Logger::Info("MobileRenderer", std::string("Loaded primary font: ") + p);
            break;
        }
    }

    const char* thaiPaths[] = {
        "/system/fonts/NotoSansThai-Regular.ttf",
        "/system/fonts/NotoSansThaiUI-Regular.ttf",
        "/system/fonts/DroidSansFallback.ttf"
    };
    for (const char* p : thaiPaths) {
        if (LoadFontFile(p, m_thaiFontData) &&
            stbtt_InitFont(&g_thaiFontInfo, m_thaiFontData.data(), 0)) {
            m_hasThaiFont = true;
            Logger::Info("MobileRenderer", std::string("Loaded Thai font: ") + p);
            break;
        }
    }

#ifdef __ANDROID__
    glViewport(0, 0, m_width, m_height);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.05f, 0.07f, 0.12f, 1.0f);

    GLuint vs = CompileShader(GL_VERTEX_SHADER, kUIVertexShader);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kUIFragmentShader);
    if (vs && fs) {
        m_uiProgram = glCreateProgram();
        glAttachShader(m_uiProgram, vs);
        glAttachShader(m_uiProgram, fs);
        glLinkProgram(m_uiProgram);
        glDeleteShader(vs);
        glDeleteShader(fs);
        m_uProjLoc = glGetUniformLocation(m_uiProgram, "uProj");
        m_uColorLoc = glGetUniformLocation(m_uiProgram, "uColor");
        m_uUseTexLoc = glGetUniformLocation(m_uiProgram, "uUseTexture");
    }

    glGenVertexArrays(1, &m_uiVAO);
    glGenBuffers(1, &m_uiVBO);
    glBindVertexArray(m_uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_uiVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 16, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
    glBindVertexArray(0);

    glGenTextures(1, &m_fontAtlasTex);
    glBindTexture(GL_TEXTURE_2D, m_fontAtlasTex);
    std::vector<uint8_t> zeros(kAtlasSize * kAtlasSize, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, kAtlasSize, kAtlasSize, 0, GL_RED, GL_UNSIGNED_BYTE, zeros.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    // B17: 3D character pass program + buffers (dynamic VBO/EBO, grown on demand).
    GLuint cvs = CompileShader(GL_VERTEX_SHADER, kCharVertexShader);
    GLuint cfs = CompileShader(GL_FRAGMENT_SHADER, kCharFragmentShader);
    if (cvs && cfs) {
        m_charProgram = glCreateProgram();
        glAttachShader(m_charProgram, cvs);
        glAttachShader(m_charProgram, cfs);
        glLinkProgram(m_charProgram);
        glDeleteShader(cvs);
        glDeleteShader(cfs);
        m_uVPLoc = glGetUniformLocation(m_charProgram, "uVP");
        m_uColorLoc3D = glGetUniformLocation(m_charProgram, "uColor");
        m_uUseTexLoc3D = glGetUniformLocation(m_charProgram, "uUseTexture");
        m_uTexLoc3D = glGetUniformLocation(m_charProgram, "uTex");
    }
    glGenVertexArrays(1, &m_charVAO);
    glGenBuffers(1, &m_charVBO);
    glGenBuffers(1, &m_charEBO);
    glBindVertexArray(m_charVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_charVBO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), reinterpret_cast<void*>(6 * sizeof(float)));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_charEBO);
    glBindVertexArray(0);

    // B18: GPU skinning program + shared palette uniform block.  256 mat4 entries
    // (kMaxGpuBones) = 16 KB, the ES 3.0 minimum guaranteed MAX_UNIFORM_BLOCK_SIZE;
    // devices below that minimum are logged and GPU-skinned draws fail loudly.
    GLuint gvs = CompileShader(GL_VERTEX_SHADER, kCharGpuVertexShader);
    GLuint gfs = CompileShader(GL_FRAGMENT_SHADER, kCharGpuFragmentShader);
    if (gvs && gfs) {
        m_charGpuProgram = glCreateProgram();
        glAttachShader(m_charGpuProgram, gvs);
        glAttachShader(m_charGpuProgram, gfs);
        glLinkProgram(m_charGpuProgram);
        glDeleteShader(gvs);
        glDeleteShader(gfs);
        GLint linked = 0;
        glGetProgramiv(m_charGpuProgram, GL_LINK_STATUS, &linked);
        if (!linked) {
            char log[512];
            glGetProgramInfoLog(m_charGpuProgram, sizeof(log), nullptr, log);
            Logger::Error("MobileRenderer", std::string("GPU skin program link error: ") + log);
            glDeleteProgram(m_charGpuProgram);
            m_charGpuProgram = 0;
        }
    }
    if (m_charGpuProgram) {
        GLint maxBlockSize = 0;
        glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &maxBlockSize);
        if (maxBlockSize < static_cast<GLint>(kMaxGpuBones * 64)) {
            Logger::Error("MobileRenderer", "GL_MAX_UNIFORM_BLOCK_SIZE (" + std::to_string(maxBlockSize) +
                          ") below the 16 KB palette requirement; GPU skinning will be skipped");
        }
        glGenBuffers(1, &m_gpuPaletteUBO);
        m_gpuPaletteUBOBytes = kMaxGpuBones * 64;
        glBindBuffer(GL_UNIFORM_BUFFER, m_gpuPaletteUBO);
        glBufferData(GL_UNIFORM_BUFFER, static_cast<GLsizeiptr>(m_gpuPaletteUBOBytes), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);
        const GLuint blockIndex = glGetUniformBlockIndex(m_charGpuProgram, "BonePaletteBlock");
        if (blockIndex != GL_INVALID_INDEX) {
            glUniformBlockBinding(m_charGpuProgram, blockIndex, 0);
        }
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_gpuPaletteUBO);
        Logger::Info("MobileRenderer", "GPU skinning program ready (palette " +
                     std::to_string(kMaxGpuBones) + " bones, " + std::to_string(maxBlockSize) + " B max block)");
    }
#endif

    m_initialized = true;
    Logger::Info("MobileRenderer", "Initialized OpenGL ES 3.0 Renderer (" +
                 std::to_string(m_width) + "x" + std::to_string(m_height) + ")");
    return true;
}

void MobileRenderer::Resize(int width, int height) {
    m_width = width > 0 ? width : 1280;
    m_height = height > 0 ? height : 720;
#ifdef __ANDROID__
    if (m_initialized) {
        glViewport(0, 0, m_width, m_height);
    }
#endif
}

void MobileRenderer::Shutdown() {
#ifdef __ANDROID__
    if (m_fontAtlasTex) { glDeleteTextures(1, &m_fontAtlasTex); m_fontAtlasTex = 0; }
    if (m_uiVBO) { glDeleteBuffers(1, &m_uiVBO); m_uiVBO = 0; }
    if (m_uiVAO) { glDeleteVertexArrays(1, &m_uiVAO); m_uiVAO = 0; }
    if (m_uiProgram) { glDeleteProgram(m_uiProgram); m_uiProgram = 0; }
    if (m_charEBO) { glDeleteBuffers(1, &m_charEBO); m_charEBO = 0; }
    if (m_charVBO) { glDeleteBuffers(1, &m_charVBO); m_charVBO = 0; }
    if (m_charVAO) { glDeleteVertexArrays(1, &m_charVAO); m_charVAO = 0; }
    if (m_charProgram) { glDeleteProgram(m_charProgram); m_charProgram = 0; }
    for (auto& kv : m_gpuMeshes) {
        GpuMeshGlState* state = static_cast<GpuMeshGlState*>(kv.second);
        if (state) {
            if (state->ebo) glDeleteBuffers(1, &state->ebo);
            if (state->vbo) glDeleteBuffers(1, &state->vbo);
            if (state->vao) glDeleteVertexArrays(1, &state->vao);
            delete state;
        }
    }
    m_gpuMeshes.clear();
    m_gpuMeshCount = 0;
    m_nextGpuMeshHandle = 1;
    if (m_gpuPaletteUBO) { glDeleteBuffers(1, &m_gpuPaletteUBO); m_gpuPaletteUBO = 0; }
    if (m_charGpuProgram) { glDeleteProgram(m_charGpuProgram); m_charGpuProgram = 0; }
#endif
    m_glyphCache.clear();
    m_scene3DActive = false;
    m_initialized = false;
}

void MobileRenderer::BeginFrame() {
    m_stats = RenderStats{};
#ifdef __ANDROID__
    if (m_initialized) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
#endif
}

void MobileRenderer::EndFrame() {
}

const GlyphInfo* MobileRenderer::GetOrLoadGlyph(uint32_t codepoint) {
    auto it = m_glyphCache.find(codepoint);
    if (it != m_glyphCache.end()) return &it->second;

    GlyphInfo info;
    stbtt_fontinfo* font = nullptr;
    if (codepoint >= 0x0E00 && codepoint <= 0x0E7F && m_hasThaiFont) {
        font = &g_thaiFontInfo;
    } else if (m_hasPrimaryFont && stbtt_FindGlyphIndex(&g_primaryFontInfo, static_cast<int>(codepoint)) != 0) {
        font = &g_primaryFontInfo;
    } else if (m_hasThaiFont && stbtt_FindGlyphIndex(&g_thaiFontInfo, static_cast<int>(codepoint)) != 0) {
        font = &g_thaiFontInfo;
    }

    if (!font) {
        info.advance = kBaseFontPx * 0.5f;
        info.valid = false;
        m_glyphCache[codepoint] = info;
        return &m_glyphCache[codepoint];
    }

    float scale = stbtt_ScaleForPixelHeight(font, kBaseFontPx);
    int adv = 0, lsb = 0;
    stbtt_GetCodepointHMetrics(font, static_cast<int>(codepoint), &adv, &lsb);
    info.advance = adv * scale;

    int x0, y0, x1, y1;
    stbtt_GetCodepointBitmapBox(font, static_cast<int>(codepoint), scale, scale, &x0, &y0, &x1, &y1);
    int gw = x1 - x0;
    int gh = y1 - y0;
    info.w = static_cast<float>(gw);
    info.h = static_cast<float>(gh);
    info.xoff = static_cast<float>(x0);
    info.yoff = static_cast<float>(y0) + kBaseFontPx * 0.8f;

    if (gw > 0 && gh > 0 && m_atlasY + gh + 2 < kAtlasSize) {
        if (m_atlasX + gw + 2 >= kAtlasSize) {
            m_atlasX = 1;
            m_atlasY += m_atlasRowH + 2;
            m_atlasRowH = 0;
        }
        if (m_atlasY + gh + 2 < kAtlasSize) {
#ifdef __ANDROID__
            std::vector<uint8_t> bmp(static_cast<size_t>(gw * gh), 0);
            stbtt_MakeCodepointBitmap(font, bmp.data(), gw, gh, gw, scale, scale, static_cast<int>(codepoint));
            glBindTexture(GL_TEXTURE_2D, m_fontAtlasTex);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexSubImage2D(GL_TEXTURE_2D, 0, m_atlasX, m_atlasY, gw, gh, GL_RED, GL_UNSIGNED_BYTE, bmp.data());
            glBindTexture(GL_TEXTURE_2D, 0);
#endif
            info.u0 = static_cast<float>(m_atlasX) / kAtlasSize;
            info.v0 = static_cast<float>(m_atlasY) / kAtlasSize;
            info.u1 = static_cast<float>(m_atlasX + gw) / kAtlasSize;
            info.v1 = static_cast<float>(m_atlasY + gh) / kAtlasSize;
            m_atlasX += gw + 2;
            if (gh > m_atlasRowH) m_atlasRowH = gh;
            info.valid = true;
        }
    }

    m_glyphCache[codepoint] = info;
    return &m_glyphCache[codepoint];
}

void MobileRenderer::DrawUIQuad(float x, float y, float width, float height, float r, float g, float b, float a) {
    m_stats.drawCalls++;
    m_stats.triangles += 2;
#ifdef __ANDROID__
    if (!m_initialized || !m_uiProgram) return;
    glUseProgram(m_uiProgram);
    float L = 0.0f, R = static_cast<float>(m_width);
    float T = 0.0f, B = static_cast<float>(m_height);
    float proj[16] = {
        2.0f / (R - L), 0.0f, 0.0f, 0.0f,
        0.0f, 2.0f / (T - B), 0.0f, 0.0f,
        0.0f, 0.0f, -1.0f, 0.0f,
        -(R + L) / (R - L), -(T + B) / (T - B), 0.0f, 1.0f
    };
    glUniformMatrix4fv(m_uProjLoc, 1, GL_FALSE, proj);
    glUniform4f(m_uColorLoc, r, g, b, a);
    glUniform1i(m_uUseTexLoc, 0);

    float verts[16] = {
        x, y, 0.0f, 0.0f,
        x + width, y, 1.0f, 0.0f,
        x, y + height, 0.0f, 1.0f,
        x + width, y + height, 1.0f, 1.0f
    };
    glBindVertexArray(m_uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_uiVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
#else
    (void)x; (void)y; (void)width; (void)height; (void)r; (void)g; (void)b; (void)a;
#endif
}

void MobileRenderer::DrawUIText(float x, float y, const std::string& text, float scale, float r, float g, float b, float a) {
    if (text.empty()) return;
    float s = scale * 0.85f;
    float cursorX = x;
    size_t idx = 0;
    while (idx < text.size()) {
        uint32_t cp = DecodeUTF8(text, idx);
        const GlyphInfo* gInfo = GetOrLoadGlyph(cp);
        if (!gInfo) continue;

        if (gInfo->valid && gInfo->w > 0 && gInfo->h > 0) {
            m_stats.drawCalls++;
            m_stats.triangles += 2;
#ifdef __ANDROID__
            if (m_initialized && m_uiProgram && m_fontAtlasTex) {
                glUseProgram(m_uiProgram);
                float L = 0.0f, R = static_cast<float>(m_width);
                float T = 0.0f, B = static_cast<float>(m_height);
                float proj[16] = {
                    2.0f / (R - L), 0.0f, 0.0f, 0.0f,
                    0.0f, 2.0f / (T - B), 0.0f, 0.0f,
                    0.0f, 0.0f, -1.0f, 0.0f,
                    -(R + L) / (R - L), -(T + B) / (T - B), 0.0f, 1.0f
                };
                glUniformMatrix4fv(m_uProjLoc, 1, GL_FALSE, proj);
                glUniform4f(m_uColorLoc, r, g, b, a);
                glUniform1i(m_uUseTexLoc, 1);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, m_fontAtlasTex);

                float gx = cursorX + gInfo->xoff * s;
                float gy = y + gInfo->yoff * s;
                float gw = gInfo->w * s;
                float gh = gInfo->h * s;
                float verts[16] = {
                    gx, gy, gInfo->u0, gInfo->v0,
                    gx + gw, gy, gInfo->u1, gInfo->v0,
                    gx, gy + gh, gInfo->u0, gInfo->v1,
                    gx + gw, gy + gh, gInfo->u1, gInfo->v1
                };
                glBindVertexArray(m_uiVAO);
                glBindBuffer(GL_ARRAY_BUFFER, m_uiVBO);
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
                glBindVertexArray(0);
                glBindTexture(GL_TEXTURE_2D, 0);
            }
#else
            (void)y; (void)r; (void)g; (void)b; (void)a;
#endif
        }
        cursorX += gInfo->advance * s;
    }
}

float MobileRenderer::MeasureTextWidth(const std::string& text, float scale) {
    float s = scale * 0.85f;
    float w = 0.0f;
    size_t idx = 0;
    while (idx < text.size()) {
        uint32_t cp = DecodeUTF8(text, idx);
        const GlyphInfo* gInfo = GetOrLoadGlyph(cp);
        if (gInfo) w += gInfo->advance * s;
    }
    return w;
}

void MobileRenderer::ApplySettings(const GraphicsSettings& settings) {
    m_settings = settings;
}

bool MobileRenderer::IsBoxInFrustum(float minX, float minY, float minZ, float maxX, float maxY, float maxZ) const {
    (void)minY;
    (void)maxY;
    float centerX = (minX + maxX) * 0.5f;
    float centerZ = (minZ + maxZ) * 0.5f;
    float distSq = centerX * centerX + centerZ * centerZ;
    return distSq <= (m_settings.viewDistance * m_settings.viewDistance);
}

uint32_t MobileRenderer::UploadRGBA8Texture(uint32_t width, uint32_t height, const uint8_t* rgba8) {
    if (!m_initialized || width == 0 || height == 0 || !rgba8) return 0;
#ifdef __ANDROID__
    GLuint texId = 0;
    glGenTextures(1, &texId);
    if (texId == 0) return 0;
    glBindTexture(GL_TEXTURE_2D, texId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(width), static_cast<GLsizei>(height),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba8);
    glBindTexture(GL_TEXTURE_2D, 0);
    return static_cast<uint32_t>(texId);
#else
    return 1; // Non-Android headless test stub
#endif
}

void MobileRenderer::DeleteTexture(uint32_t texId) {
    if (texId == 0) return;
#ifdef __ANDROID__
    GLuint id = static_cast<GLuint>(texId);
    glDeleteTextures(1, &id);
#endif
}

// ---------------------------------------------------------------------------
// B17: 3D character pass (see MobileRenderer.h).
// Camera math uses GL column-major matrices (transpose of the D3D row-vector
// convention the animation pipeline uses); EP7Math stays render-agnostic.
// ---------------------------------------------------------------------------
namespace {

struct GlMat4 {
    float m[16] = {0};
    GlMat4() { m[0] = m[5] = m[10] = m[15] = 1.0f; }
    float& operator[](size_t i) { return m[i]; }
    const float& operator[](size_t i) const { return m[i]; }
};

GlMat4 GlPerspective(float fovYRadians, float aspect, float nearZ, float farZ) {
    GlMat4 out;
    const float f = 1.0f / std::tan(fovYRadians * 0.5f);
    out[0] = f / aspect;
    out[5] = f;
    out[10] = (farZ + nearZ) / (nearZ - farZ);
    out[11] = -1.0f;
    out[14] = (2.0f * farZ * nearZ) / (nearZ - farZ);
    out[15] = 0.0f;
    return out;
}

void GlNormalize(float* v) {
    const float len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (len <= 1e-9f) return;
    v[0] /= len;
    v[1] /= len;
    v[2] /= len;
}

GlMat4 GlLookAt(const float* eye, const float* center, const float* up) {
    float f[3] = {center[0] - eye[0], center[1] - eye[1], center[2] - eye[2]};
    GlNormalize(f);
    float s[3] = {
        f[1] * up[2] - f[2] * up[1],
        f[2] * up[0] - f[0] * up[2],
        f[0] * up[1] - f[1] * up[0]
    };
    GlNormalize(s);
    float u[3] = {
        s[1] * f[2] - s[2] * f[1],
        s[2] * f[0] - s[0] * f[2],
        s[0] * f[1] - s[1] * f[0]
    };
    GlMat4 out;
    out[0] = s[0];  out[4] = s[1];  out[8]  = s[2];
    out[1] = u[0];  out[5] = u[1];  out[9]  = u[2];
    out[2] = -f[0]; out[6] = -f[1]; out[10] = -f[2];
    out[12] = -(s[0] * eye[0] + s[1] * eye[1] + s[2] * eye[2]);
    out[13] = -(u[0] * eye[0] + u[1] * eye[1] + u[2] * eye[2]);
    out[14] = (f[0] * eye[0] + f[1] * eye[1] + f[2] * eye[2]);
    return out;
}

GlMat4 GlMultiply(const GlMat4& a, const GlMat4& b) {
    GlMat4 out;
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a.m[k * 4 + r] * b.m[c * 4 + k];
            }
            out.m[c * 4 + r] = sum;
        }
    }
    return out;
}

}  // namespace

bool MobileRenderer::BeginScene3D(const MobileCamera& camera) {
    // Orbit eye position from the MobileCamera state (MobileCamera::Update semantics:
    // yaw/pitch in degrees, distance back from the target).
    const float yawRad = camera.GetYaw() * 3.14159265358979323846f / 180.0f;
    const float pitchRad = camera.GetPitch() * 3.14159265358979323846f / 180.0f;
    float target[3] = {0.0f, 10.0f, 0.0f};  // ~waist height of the authentic bounds box

    float eye[3] = {
        target[0] + camera.GetDistance() * std::cos(pitchRad) * std::sin(yawRad),
        target[1] + camera.GetDistance() * std::sin(pitchRad),
        target[2] + camera.GetDistance() * std::cos(pitchRad) * std::cos(yawRad)
    };
    const float up[3] = {0.0f, 1.0f, 0.0f};

    const float aspect = (m_height > 0) ? static_cast<float>(m_width) / static_cast<float>(m_height) : 1.0f;
    const float farZ = (m_settings.viewDistance > 0.0f) ? m_settings.viewDistance * 4.0f : 500.0f;
    const GlMat4 proj = GlPerspective(45.0f * 3.14159265358979323846f / 180.0f, aspect, 0.1f, farZ);
    const GlMat4 view = GlLookAt(eye, target, up);
    const GlMat4 vp = GlMultiply(proj, view);

    m_scene3DActive = true;
#ifdef __ANDROID__
    if (m_initialized && m_charProgram) {
        for (int i = 0; i < 16; ++i) m_viewProj[i] = vp.m[i];
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glDisable(GL_BLEND);
        glUseProgram(m_charProgram);
        glUniformMatrix4fv(m_uVPLoc, 1, GL_FALSE, m_viewProj);
        glUniform4f(m_uColorLoc3D, 0.75f, 0.75f, 0.75f, 1.0f);
        glUniform1i(m_uUseTexLoc3D, 0);
        glUniform1i(m_uTexLoc3D, 0);
        return true;
    }
#else
    (void)vp;
#endif
    return m_scene3DActive;
}

void MobileRenderer::EndScene3D() {
    m_scene3DActive = false;
#ifdef __ANDROID__
    if (m_initialized) {
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glUseProgram(m_uiProgram);
    }
#endif
}

bool MobileRenderer::DrawSkinnedMesh(const SkinnedDrawInput& input) {
    if (!m_scene3DActive) return false;
    if (!input.positions || !input.indices || input.vertexCount == 0 || input.indexCount == 0) return false;
    if (input.indexCount % 3 != 0) return false;

    m_stats.drawCalls++;
    m_stats.triangles += input.indexCount / 3;

#ifdef __ANDROID__
    if (!m_initialized || !m_charProgram) return false;

    // Interleave position + normal + uv into the dynamic VBO (grown on demand).
    const bool hasUV = input.texcoords != nullptr;
    const size_t strideFloats = 8;
    const size_t needed = static_cast<size_t>(input.vertexCount) * strideFloats * sizeof(float);
    if (needed > m_charVBOBytes) {
        m_charVBOBytes = needed + needed / 2;  // grow geometrically, no per-frame realloc
        glBindBuffer(GL_ARRAY_BUFFER, m_charVBO);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_charVBOBytes), nullptr, GL_DYNAMIC_DRAW);
    }
    std::vector<float> staging(static_cast<size_t>(input.vertexCount) * strideFloats);
    for (uint32_t v = 0; v < input.vertexCount; ++v) {
        float* dst = staging.data() + static_cast<size_t>(v) * strideFloats;
        dst[0] = input.positions[v * 3 + 0];
        dst[1] = input.positions[v * 3 + 1];
        dst[2] = input.positions[v * 3 + 2];
        dst[3] = input.normals ? input.normals[v * 3 + 0] : 0.0f;
        dst[4] = input.normals ? input.normals[v * 3 + 1] : 1.0f;
        dst[5] = input.normals ? input.normals[v * 3 + 2] : 0.0f;
        dst[6] = hasUV ? input.texcoords[v * 2 + 0] : 0.0f;
        dst[7] = hasUV ? input.texcoords[v * 2 + 1] : 0.0f;
    }
    const size_t eboNeeded = static_cast<size_t>(input.indexCount) * sizeof(uint32_t);
    if (eboNeeded > m_charEBOBytes) {
        m_charEBOBytes = eboNeeded + eboNeeded / 2;
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_charEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(m_charEBOBytes), nullptr, GL_DYNAMIC_DRAW);
    }

    glBindVertexArray(m_charVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_charVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(needed), staging.data());
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_charEBO);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(eboNeeded), input.indices);

    glUseProgram(m_charProgram);
    glUniformMatrix4fv(m_uVPLoc, 1, GL_FALSE, m_viewProj);
    glUniform4f(m_uColorLoc3D, input.colorR, input.colorG, input.colorB, input.colorA);
    if (input.textureId != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, input.textureId);
        glUniform1i(m_uTexLoc3D, 0);
        glUniform1i(m_uUseTexLoc3D, 1);
    } else {
        glUniform1i(m_uUseTexLoc3D, 0);
    }
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(input.indexCount), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
#endif
    return true;
}

// ---------------------------------------------------------------------------
// B18: GPU skinned mesh upload/draw/release (authentic D3DINDEXEDHLSLVS parity).
// ---------------------------------------------------------------------------

uint32_t MobileRenderer::UploadGpuSkinnedMesh(const GpuSkinnedMeshData& data) {
    if (!data.interleaved || !data.indices || data.vertexCount == 0 || data.indexCount == 0) return 0;
    if (data.indexCount % 3 != 0) return 0;

    auto* state = new GpuMeshGlState();
    state->vertexCount = data.vertexCount;
    state->indexCount = data.indexCount;

#ifdef __ANDROID__
    if (m_initialized) {
        const GLsizeiptr vboBytes = static_cast<GLsizeiptr>(data.vertexCount) * 16 * sizeof(float);
        const GLsizeiptr eboBytes = static_cast<GLsizeiptr>(data.indexCount) * sizeof(uint32_t);
        glGenVertexArrays(1, &state->vao);
        glGenBuffers(1, &state->vbo);
        glGenBuffers(1, &state->ebo);
        glBindVertexArray(state->vao);
        glBindBuffer(GL_ARRAY_BUFFER, state->vbo);
        glBufferData(GL_ARRAY_BUFFER, vboBytes, data.interleaved, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 16 * sizeof(float), reinterpret_cast<void*>(0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 16 * sizeof(float), reinterpret_cast<void*>(3 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 16 * sizeof(float), reinterpret_cast<void*>(6 * sizeof(float)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 16 * sizeof(float), reinterpret_cast<void*>(8 * sizeof(float)));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 16 * sizeof(float), reinterpret_cast<void*>(12 * sizeof(float)));
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, state->ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, eboBytes, data.indices, GL_STATIC_DRAW);
        glBindVertexArray(0);
    }
#endif

    const uint32_t handle = m_nextGpuMeshHandle++;
    m_gpuMeshes[handle] = state;
    ++m_gpuMeshCount;
    return handle;
}

void MobileRenderer::ReleaseGpuSkinnedMesh(uint32_t handle) {
    auto it = m_gpuMeshes.find(handle);
    if (it == m_gpuMeshes.end()) return;
    auto* state = static_cast<GpuMeshGlState*>(it->second);
#ifdef __ANDROID__
    if (m_initialized && state) {
        if (state->ebo) glDeleteBuffers(1, &state->ebo);
        if (state->vbo) glDeleteBuffers(1, &state->vbo);
        if (state->vao) glDeleteVertexArrays(1, &state->vao);
    }
#endif
    delete state;
    m_gpuMeshes.erase(it);
    if (m_gpuMeshCount > 0) --m_gpuMeshCount;
}

bool MobileRenderer::DrawGpuSkinnedMesh(uint32_t handle,
                                        const float* paletteRowMajor,
                                        uint32_t paletteCount,
                                        uint32_t textureId,
                                        const float color[4],
                                        bool alphaBlend,
                                        uint32_t faceStart,
                                        uint32_t faceCount) {
    if (!m_scene3DActive) return false;
    auto it = m_gpuMeshes.find(handle);
    if (it == m_gpuMeshes.end() || !it->second) {
        Logger::Error("MobileRenderer", "DrawGpuSkinnedMesh: unknown mesh handle");
        ++m_gpuDrawFailures;
        return false;
    }
    auto* state = static_cast<GpuMeshGlState*>(it->second);
    if (!paletteRowMajor || paletteCount == 0 || paletteCount > kMaxGpuBones) {
        Logger::Error("MobileRenderer", "DrawGpuSkinnedMesh: palette overflow/invalid (count " +
                      std::to_string(paletteCount) + ", max " + std::to_string(kMaxGpuBones) + ")");
        ++m_gpuDrawFailures;
        return false;
    }
    const uint64_t firstIndex = static_cast<uint64_t>(faceStart) * 3;
    const uint64_t drawIndices = static_cast<uint64_t>(faceCount) * 3;
    if (firstIndex + drawIndices > state->indexCount) {
        Logger::Error("MobileRenderer", "DrawGpuSkinnedMesh: face range out of bounds (start " +
                      std::to_string(faceStart) + ", count " + std::to_string(faceCount) +
                      ", mesh indices " + std::to_string(state->indexCount) + ")");
        ++m_gpuDrawFailures;
        return false;
    }

    ++m_gpuDrawCount;
    m_stats.drawCalls++;
    m_stats.triangles += faceCount;

#ifndef __ANDROID__
    (void)textureId;
    (void)color;
    (void)alphaBlend;
    return true;
#else
    if (!m_initialized || !m_charGpuProgram) return false;

    // Palette upload (offset*combined, row-major — DxSkinMesh9_HLSL.cpp:220-229
    // "mWorldMatrixArray" parity) into the shared uniform block; only the live
    // entries are written, entries beyond paletteCount stay stale but are
    // unreachable (the streams reference indices < mesh bone count = paletteCount).
    glBindBuffer(GL_UNIFORM_BUFFER, m_gpuPaletteUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, static_cast<GLsizeiptr>(paletteCount) * 64, paletteRowMajor);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glUseProgram(m_charGpuProgram);
    glUniformMatrix4fv(glGetUniformLocation(m_charGpuProgram, "uVP"), 1, GL_FALSE, m_viewProj);
    glUniform4f(glGetUniformLocation(m_charGpuProgram, "uColor"),
                color ? color[0] : 0.75f, color ? color[1] : 0.75f,
                color ? color[2] : 0.75f, color ? color[3] : 1.0f);
    if (textureId != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureId);
        glUniform1i(glGetUniformLocation(m_charGpuProgram, "uTex"), 0);
        glUniform1i(glGetUniformLocation(m_charGpuProgram, "uUseTexture"), 1);
    } else {
        glUniform1i(glGetUniformLocation(m_charGpuProgram, "uUseTexture"), 0);
    }
    if (alphaBlend) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }
    glBindVertexArray(state->vao);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(drawIndices), GL_UNSIGNED_INT,
                   reinterpret_cast<const void*>(firstIndex * sizeof(uint32_t)));
    glBindVertexArray(0);
    if (alphaBlend) glDisable(GL_BLEND);
#endif
    return true;
}

} // namespace RANMobile
