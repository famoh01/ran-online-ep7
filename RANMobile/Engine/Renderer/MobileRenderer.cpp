#include "MobileRenderer.h"
#include "../Core/Logger.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <fstream>

#ifdef __ANDROID__
#include <GLES3/gl3.h>
#endif

namespace RANMobile {

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
uniform float uAlphaCutoff;
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
    if (FragColor.a < uAlphaCutoff) discard;
}
)";

static const char* kWorldVertexShader = R"(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec4 aColor;
uniform mat4 uViewProj;
uniform mat4 uModel;
out vec3 vWorldPos;
out vec3 vWorldNormal;
out vec2 vUV;
out vec4 vColor;
void main() {
    vec4 wp = uModel * vec4(aPos, 1.0);
    gl_Position = uViewProj * wp;
    vWorldPos = wp.xyz;
    vWorldNormal = mat3(uModel) * aNormal;
    vUV = aUV;
    vColor = aColor;
}
)";

static const char* kWorldFragmentShader = R"(#version 300 es
precision highp float;
uniform sampler2D uTex;
uniform int uHasTex;
uniform float uAlphaCutoff;
uniform vec4 uTint;
uniform int uUnlit;
uniform vec3 uCameraPos;
uniform vec3 uAmbientColor;
uniform vec3 uDirLightDir;
uniform vec3 uDirLightColor;
uniform int uPointLightCount;
uniform vec4 uPointPosInvRange[4];
uniform vec3 uPointColor[4];
uniform int uFogEnabled;
uniform float uFogStart;
uniform float uFogEnd;
uniform vec3 uFogColor;
in vec3 vWorldPos;
in vec3 vWorldNormal;
in vec2 vUV;
in vec4 vColor;
out vec4 FragColor;
void main() {
    vec4 albedo = vColor * uTint;
    if (uHasTex != 0) {
        albedo *= texture(uTex, vUV);
    }
    if (albedo.a < uAlphaCutoff) discard;
    vec3 lit = albedo.rgb;
    if (uUnlit == 0) {
        float nLen = length(vWorldNormal);
        vec3 N = (nLen > 0.001) ? (vWorldNormal / nLen) : vec3(0.0, 1.0, 0.0);
        vec3 lightSum = uAmbientColor;
        float dLen = length(uDirLightDir);
        if (dLen > 0.001) {
            vec3 Ldir = -uDirLightDir / dLen;
            float ndl = max(dot(N, Ldir), 0.0);
            lightSum += uDirLightColor * ndl;
        }
        for (int i = 0; i < 4; ++i) {
            if (i >= uPointLightCount) break;
            vec3 toL = uPointPosInvRange[i].xyz - vWorldPos;
            float dist = length(toL);
            float atten = clamp(1.0 - dist * uPointPosInvRange[i].w, 0.0, 1.0);
            if (atten > 0.0 && dist > 0.0001) {
                float pndl = max(dot(N, toL / dist), 0.0);
                lightSum += uPointColor[i] * (atten * atten * (0.35 + 0.65 * pndl));
            }
        }
        lit *= clamp(lightSum, 0.0, 2.0);
    }
    if (uFogEnabled != 0) {
        float dist = length(vWorldPos - uCameraPos);
        float fogFactor = clamp((dist - uFogStart) / max(uFogEnd - uFogStart, 0.001), 0.0, 1.0);
        lit = mix(lit, uFogColor, fogFactor);
    }
    FragColor = vec4(lit, albedo.a);
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

static GLuint LinkProgram(const char* vsSrc, const char* fsSrc, const char* label) {
    GLuint vs = CompileShader(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fsSrc);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        Logger::Error("MobileRenderer", std::string(label) + " shader compilation failed");
        return 0;
    }
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint linked = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[512];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        Logger::Error("MobileRenderer", std::string(label) + " link failed: " + log);
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

static void CheckGLError(const char* operation) {
    GLenum err = glGetError();
    if (err == GL_NO_ERROR) return;
    const char* name = "UNKNOWN";
    switch (err) {
        case GL_INVALID_ENUM: name = "GL_INVALID_ENUM"; break;
        case GL_INVALID_VALUE: name = "GL_INVALID_VALUE"; break;
        case GL_INVALID_OPERATION: name = "GL_INVALID_OPERATION"; break;
        case GL_OUT_OF_MEMORY: name = "GL_OUT_OF_MEMORY"; break;
        default: break;
    }
    Logger::Error("MobileRenderer", std::string(operation) + " failed: " + name);
}

static void TransposeD3DToGL(const EP7Mat4& rowMajor, float outColMajor[16]) {
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            // D3DXMATRIX is row-major row-vector (v * M); GLSL uses column-vector (M^T * v).
            // Since M[r][c] in row-major sits at index r*4+c, putting M[r][c] into out[r*4+c]
            // directly yields column-major M^T!
            outColMajor[r * 4 + c] = rowMajor.m[static_cast<size_t>(r) * 4u + static_cast<size_t>(c)];
        }
    }
}
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

void MobileRenderer::UpdateProjection() {
    const float pw = static_cast<float>(m_layout.GetPhysicalWidth());
    const float ph = static_cast<float>(m_layout.GetPhysicalHeight());
    const float sx = m_layout.GetScale();
    const float sy = m_layout.GetScale();
    const float ox = m_layout.GetOffsetX();
    const float oy = m_layout.GetOffsetY();
    for (float& v : m_projection) v = 0.0f;
    m_projection[0] = (pw > 0.0f) ? 2.0f * sx / pw : 0.0f;
    m_projection[5] = (ph > 0.0f) ? -2.0f * sy / ph : 0.0f;
    m_projection[10] = -1.0f;
    m_projection[12] = (pw > 0.0f) ? 2.0f * ox / pw - 1.0f : 0.0f;
    m_projection[13] = (ph > 0.0f) ? 1.0f - 2.0f * oy / ph : 0.0f;
    m_projection[15] = 1.0f;
}

void MobileRenderer::ResetGlyphAtlas() {
    m_glyphCache.clear();
    m_atlasX = 1;
    m_atlasY = 1;
    m_atlasRowH = 0;
}

bool MobileRenderer::UploadMesh3DGPU(CachedMesh3D& mesh) {
#ifdef __ANDROID__
    if (!m_gpuReady || mesh.vertices.empty() || mesh.indices.empty()) return false;
    if (mesh.vao != 0) {
        GLuint v = static_cast<GLuint>(mesh.vao);
        glDeleteVertexArrays(1, &v);
        mesh.vao = 0;
    }
    if (mesh.vbo != 0) {
        GLuint b = static_cast<GLuint>(mesh.vbo);
        glDeleteBuffers(1, &b);
        mesh.vbo = 0;
    }
    if (mesh.ibo != 0) {
        GLuint b = static_cast<GLuint>(mesh.ibo);
        glDeleteBuffers(1, &b);
        mesh.ibo = 0;
    }

    GLuint vao = 0, vbo = 0, ibo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ibo);
    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(EP7WorldVertex)),
                 mesh.vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(uint16_t)),
                 mesh.indices.data(), GL_STATIC_DRAW);

    const GLsizei stride = static_cast<GLsizei>(sizeof(EP7WorldVertex));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(EP7WorldVertex, x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(EP7WorldVertex, nx)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
                          reinterpret_cast<void*>(offsetof(EP7WorldVertex, u)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride,
                          reinterpret_cast<void*>(offsetof(EP7WorldVertex, rgba)));

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    mesh.vao = static_cast<uint32_t>(vao);
    mesh.vbo = static_cast<uint32_t>(vbo);
    mesh.ibo = static_cast<uint32_t>(ibo);
    return true;
#else
    mesh.vao = mesh.handle;
    mesh.vbo = mesh.handle;
    mesh.ibo = mesh.handle;
    return true;
#endif
}

bool MobileRenderer::CreateGPUResources() {
#ifdef __ANDROID__
    m_uiProgram = LinkProgram(kUIVertexShader, kUIFragmentShader, "UI");
    if (!m_uiProgram) {
        Logger::Error("MobileRenderer", "UI shader compilation failed; rendering disabled for this surface");
        return false;
    }
    m_uProjLoc = glGetUniformLocation(m_uiProgram, "uProj");
    m_uColorLoc = glGetUniformLocation(m_uiProgram, "uColor");
    m_uUseTexLoc = glGetUniformLocation(m_uiProgram, "uUseTexture");
    m_uAlphaCutoffLoc = glGetUniformLocation(m_uiProgram, "uAlphaCutoff");

    m_worldProgram = LinkProgram(kWorldVertexShader, kWorldFragmentShader, "World3D");
    if (m_worldProgram) {
        m_wViewProjLoc = glGetUniformLocation(m_worldProgram, "uViewProj");
        m_wModelLoc = glGetUniformLocation(m_worldProgram, "uModel");
        m_wCameraPosLoc = glGetUniformLocation(m_worldProgram, "uCameraPos");
        m_wTexLoc = glGetUniformLocation(m_worldProgram, "uTex");
        m_wHasTexLoc = glGetUniformLocation(m_worldProgram, "uHasTex");
        m_wAlphaCutoffLoc = glGetUniformLocation(m_worldProgram, "uAlphaCutoff");
        m_wTintLoc = glGetUniformLocation(m_worldProgram, "uTint");
        m_wUnlitLoc = glGetUniformLocation(m_worldProgram, "uUnlit");
        m_wAmbientLoc = glGetUniformLocation(m_worldProgram, "uAmbientColor");
        m_wDirDirLoc = glGetUniformLocation(m_worldProgram, "uDirLightDir");
        m_wDirColorLoc = glGetUniformLocation(m_worldProgram, "uDirLightColor");
        m_wPointCountLoc = glGetUniformLocation(m_worldProgram, "uPointLightCount");
        m_wPointPosInvRangeLoc = glGetUniformLocation(m_worldProgram, "uPointPosInvRange");
        m_wPointColorLoc = glGetUniformLocation(m_worldProgram, "uPointColor");
        m_wFogEnabledLoc = glGetUniformLocation(m_worldProgram, "uFogEnabled");
        m_wFogStartLoc = glGetUniformLocation(m_worldProgram, "uFogStart");
        m_wFogEndLoc = glGetUniformLocation(m_worldProgram, "uFogEnd");
        m_wFogColorLoc = glGetUniformLocation(m_worldProgram, "uFogColor");
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
    std::vector<uint8_t> zeros(static_cast<size_t>(kAtlasSize * kAtlasSize), 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, kAtlasSize, kAtlasSize, 0, GL_RED, GL_UNSIGNED_BYTE, zeros.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    glViewport(0, 0, m_layout.GetPhysicalWidth(), m_layout.GetPhysicalHeight());
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(m_clearColor[0], m_clearColor[1], m_clearColor[2], m_clearColor[3]);
    CheckGLError("CreateGPUResources");
#endif
    m_gpuReady = true;
    for (auto& entry : m_assetTextures) {
        CachedAssetTexture& cached = entry.second;
        cached.gpuId = UploadRGBA8Texture(cached.width, cached.height, cached.rgba8.data(),
                                          cached.repeatWrap, cached.generateMips);
        if (cached.gpuId == 0) {
            Logger::Error("MobileRenderer", "Failed to restore asset texture: " + entry.first);
        }
    }
    for (auto& entry : m_meshes3D) {
        UploadMesh3DGPU(entry.second);
    }
    return true;
}

void MobileRenderer::DestroyGPUResources(bool deleteObjects) {
#ifdef __ANDROID__
    if (deleteObjects) {
        for (uint32_t tex : m_ownedTextures) {
            GLuint id = static_cast<GLuint>(tex);
            glDeleteTextures(1, &id);
        }
        for (auto& entry : m_meshes3D) {
            if (entry.second.vao) { GLuint v = entry.second.vao; glDeleteVertexArrays(1, &v); }
            if (entry.second.vbo) { GLuint b = entry.second.vbo; glDeleteBuffers(1, &b); }
            if (entry.second.ibo) { GLuint b = entry.second.ibo; glDeleteBuffers(1, &b); }
        }
        if (m_fontAtlasTex) glDeleteTextures(1, &m_fontAtlasTex);
        if (m_uiVBO) glDeleteBuffers(1, &m_uiVBO);
        if (m_uiVAO) glDeleteVertexArrays(1, &m_uiVAO);
        if (m_uiProgram) glDeleteProgram(m_uiProgram);
        if (m_worldProgram) glDeleteProgram(m_worldProgram);
    }
    m_fontAtlasTex = 0;
    m_uiVBO = 0;
    m_uiVAO = 0;
    m_uiProgram = 0;
    m_uProjLoc = -1;
    m_uColorLoc = -1;
    m_uUseTexLoc = -1;
    m_uAlphaCutoffLoc = -1;
    m_worldProgram = 0;
    m_wViewProjLoc = -1;
    m_wModelLoc = -1;
    m_wCameraPosLoc = -1;
    m_wTexLoc = -1;
    m_wHasTexLoc = -1;
    m_wAlphaCutoffLoc = -1;
    m_wTintLoc = -1;
    m_wUnlitLoc = -1;
    m_wAmbientLoc = -1;
    m_wDirDirLoc = -1;
    m_wDirColorLoc = -1;
    m_wPointCountLoc = -1;
    m_wPointPosInvRangeLoc = -1;
    m_wPointColorLoc = -1;
    m_wFogEnabledLoc = -1;
    m_wFogStartLoc = -1;
    m_wFogEndLoc = -1;
    m_wFogColorLoc = -1;
#else
    (void)deleteObjects;
#endif
    m_ownedTextures.clear();
    for (auto& entry : m_assetTextures) entry.second.gpuId = 0;
    for (auto& entry : m_meshes3D) {
        entry.second.vao = 0;
        entry.second.vbo = 0;
        entry.second.ibo = 0;
    }
    m_worldPassActive = false;
    m_lastBoundWorldTex = 0xFFFFFFFFu;
    m_gpuReady = false;
    ResetGlyphAtlas();
}

bool MobileRenderer::HasGPUContext() const {
    return m_initialized && m_gpuReady;
}

bool MobileRenderer::Initialize(int width, int height) {
    if (m_initialized) return true;

    m_layout.SetPhysicalSize(width, height);
    UpdateProjection();
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
    if (!m_hasThaiFont) {
        Logger::Warn("MobileRenderer", "No Thai system font found; Thai glyphs use metric-only fallback");
    }

    m_initialized = true;
    if (!CreateGPUResources()) {
        m_initialized = false;
        Logger::Error("MobileRenderer", "Renderer initialization failed at GPU resource creation");
        return false;
    }

    Logger::Info("MobileRenderer", "Initialized GLES3 UI + 3D World renderer, physical " +
                 std::to_string(m_layout.GetPhysicalWidth()) + "x" +
                 std::to_string(m_layout.GetPhysicalHeight()) + ", design " +
                 std::to_string(static_cast<int>(m_layout.GetDesignWidth())) + "x" +
                 std::to_string(static_cast<int>(m_layout.GetDesignHeight())));
    return true;
}

void MobileRenderer::OnSurfaceCreated(int width, int height) {
    m_layout.SetPhysicalSize(width, height);
    UpdateProjection();
    if (!m_initialized) {
        Initialize(width, height);
        return;
    }
    DestroyGPUResources(false);
    if (!CreateGPUResources()) {
        Logger::Error("MobileRenderer", "Surface recreation failed; frame rendering suspended");
        return;
    }
    Logger::Info("MobileRenderer", "Surface recreated " +
                 std::to_string(width) + "x" + std::to_string(height) + "; resources restored");
}

void MobileRenderer::OnSurfaceLost() {
    if (!m_initialized) return;
    DestroyGPUResources(false);
    Logger::Info("MobileRenderer", "Surface lost; GPU resources released and glyph cache cleared");
}

void MobileRenderer::Resize(int width, int height) {
    m_layout.SetPhysicalSize(width, height);
    UpdateProjection();
#ifdef __ANDROID__
    if (m_initialized) {
        glViewport(0, 0, m_layout.GetPhysicalWidth(), m_layout.GetPhysicalHeight());
        CheckGLError("Resize/glViewport");
    }
#endif
}

void MobileRenderer::Shutdown() {
    if (!m_initialized) return;
    DestroyGPUResources(true);
    m_primaryFontData.clear();
    m_thaiFontData.clear();
    m_glyphScratch.clear();
    m_assetTextures.clear();
    m_meshes3D.clear();
    m_hasPrimaryFont = false;
    m_hasThaiFont = false;
    m_hasCamera3D = false;
    m_initialized = false;
    Logger::Info("MobileRenderer", "Renderer shut down");
}

void MobileRenderer::SetClearColor(float r, float g, float b, float a) {
    m_clearColor[0] = r;
    m_clearColor[1] = g;
    m_clearColor[2] = b;
    m_clearColor[3] = a;
#ifdef __ANDROID__
    if (HasGPUContext()) glClearColor(r, g, b, a);
#endif
}

bool MobileRenderer::BeginFrame() {
    m_stats = RenderStats{};
    m_worldPassActive = false;
#ifdef __ANDROID__
    if (!m_initialized || !HasGPUContext()) return false;
    glDepthMask(GL_TRUE);
    glClearColor(m_clearColor[0], m_clearColor[1], m_clearColor[2], m_clearColor[3]);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    CheckGLError("BeginFrame/glClear");
#endif
    return HasGPUContext();
}

void MobileRenderer::EndFrame() {
    if (m_worldPassActive) EndWorldPass();
#ifdef __ANDROID__
    if (m_initialized && HasGPUContext()) CheckGLError("EndFrame");
#endif
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
    (void)lsb;
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
            if (HasGPUContext()) {
                m_glyphScratch.assign(static_cast<size_t>(gw * gh), 0);
                stbtt_MakeCodepointBitmap(font, m_glyphScratch.data(), gw, gh, gw, scale, scale,
                                          static_cast<int>(codepoint));
                glBindTexture(GL_TEXTURE_2D, m_fontAtlasTex);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glTexSubImage2D(GL_TEXTURE_2D, 0, m_atlasX, m_atlasY, gw, gh, GL_RED,
                                GL_UNSIGNED_BYTE, m_glyphScratch.data());
                glBindTexture(GL_TEXTURE_2D, 0);
            }
#endif
            info.u0 = static_cast<float>(m_atlasX) / kAtlasSize;
            info.v0 = static_cast<float>(m_atlasY) / kAtlasSize;
            info.u1 = static_cast<float>(m_atlasX + gw) / kAtlasSize;
            info.v1 = static_cast<float>(m_atlasY + gh) / kAtlasSize;
            m_atlasX += gw + 2;
            if (gh > m_atlasRowH) m_atlasRowH = gh;
            info.valid = HasGPUContext();
        }
    }

    m_glyphCache[codepoint] = info;
    return &m_glyphCache[codepoint];
}

void MobileRenderer::DrawUIQuad(float x, float y, float width, float height, float r, float g, float b, float a) {
    m_stats.drawCalls++;
    m_stats.triangles += 2;
#ifdef __ANDROID__
    if (!HasGPUContext()) return;
    glUseProgram(m_uiProgram);
    glUniformMatrix4fv(m_uProjLoc, 1, GL_FALSE, m_projection);
    glUniform4f(m_uColorLoc, r, g, b, a);
    glUniform1i(m_uUseTexLoc, 0);
    glUniform1f(m_uAlphaCutoffLoc, 0.0f);

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
            if (HasGPUContext()) {
                glUseProgram(m_uiProgram);
                glUniformMatrix4fv(m_uProjLoc, 1, GL_FALSE, m_projection);
                glUniform4f(m_uColorLoc, r, g, b, a);
                glUniform1i(m_uUseTexLoc, 1);
                glUniform1f(m_uAlphaCutoffLoc, 0.0f);
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

void MobileRenderer::SetCameraMatrices(const MobileCamera& camera) {
    camera.GetEyePosition(m_cameraPos[0], m_cameraPos[1], m_cameraPos[2]);
    camera.ComputeViewMatrix(m_viewMatrix);
    camera.ComputeProjectionMatrix(m_projMatrix);
    camera.ComputeViewProjectionMatrix(m_viewProjMatrix);
    camera.ExtractFrustumPlanes(m_frustumPlanes);
    m_hasCamera3D = true;
}

bool MobileRenderer::IsBoxInFrustum(float minX, float minY, float minZ, float maxX, float maxY, float maxZ) const {
    if (!m_hasCamera3D) {
        float centerX = (minX + maxX) * 0.5f;
        float centerZ = (minZ + maxZ) * 0.5f;
        float distSq = centerX * centerX + centerZ * centerZ;
        return distSq <= (m_settings.viewDistance * m_settings.viewDistance);
    }

    // Closest point on AABB to camera position for accurate view-distance culling
    const float cx = std::clamp(m_cameraPos[0], minX, maxX);
    const float cy = std::clamp(m_cameraPos[1], minY, maxY);
    const float cz = std::clamp(m_cameraPos[2], minZ, maxZ);
    const float dx = cx - m_cameraPos[0];
    const float dy = cy - m_cameraPos[1];
    const float dz = cz - m_cameraPos[2];
    const float maxDist = GetEffectiveWorldViewDistance();
    if ((dx * dx + dy * dy + dz * dz) > (maxDist * maxDist)) {
        return false;
    }

    for (const FrustumPlane& p : m_frustumPlanes) {
        const float px = (p.nx >= 0.0f) ? maxX : minX;
        const float py = (p.ny >= 0.0f) ? maxY : minY;
        const float pz = (p.nz >= 0.0f) ? maxZ : minZ;
        if ((p.nx * px + p.ny * py + p.nz * pz + p.d) < 0.0f) {
            return false;
        }
    }
    return true;
}

uint32_t MobileRenderer::UploadRGBA8Texture(uint32_t width, uint32_t height, const uint8_t* rgba8,
                                            bool repeatWrap, bool generateMips) {
    if (!m_initialized || width == 0 || height == 0 || !rgba8) return 0;
#ifdef __ANDROID__
    if (!HasGPUContext()) return 0;
    GLuint texId = 0;
    glGenTextures(1, &texId);
    if (texId == 0) return 0;
    glBindTexture(GL_TEXTURE_2D, texId);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(width), static_cast<GLsizei>(height),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba8);
    if (generateMips) {
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    const GLint wrapMode = repeatWrap ? GL_REPEAT : GL_CLAMP_TO_EDGE;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapMode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapMode);
    glBindTexture(GL_TEXTURE_2D, 0);
    CheckGLError("UploadRGBA8Texture");
    m_ownedTextures.push_back(static_cast<uint32_t>(texId));
    return static_cast<uint32_t>(texId);
#else
    (void)repeatWrap;
    (void)generateMips;
    uint32_t stubId = static_cast<uint32_t>(m_ownedTextures.size() + 1);
    m_ownedTextures.push_back(stubId);
    return stubId;
#endif
}

void MobileRenderer::DeleteTexture(uint32_t texId) {
    if (texId == 0) return;
    for (auto it = m_ownedTextures.begin(); it != m_ownedTextures.end(); ++it) {
        if (*it == texId) {
#ifdef __ANDROID__
            if (HasGPUContext()) {
                GLuint id = static_cast<GLuint>(texId);
                glDeleteTextures(1, &id);
            }
#endif
            m_ownedTextures.erase(it);
            return;
        }
    }
}

bool MobileRenderer::LoadAssetTexture(const std::string& key, uint32_t width, uint32_t height,
                                      const std::vector<uint8_t>& rgba8,
                                      bool repeatWrap, bool generateMips) {
    if (key.empty() || width == 0 || height == 0 ||
        rgba8.size() < static_cast<size_t>(width) * height * 4) return false;
    auto it = m_assetTextures.find(key);
    if (it != m_assetTextures.end()) {
        if (it->second.width == width && it->second.height == height &&
            it->second.repeatWrap == repeatWrap &&
            it->second.generateMips == generateMips &&
            it->second.rgba8 == rgba8 && it->second.gpuId != 0) {
            return true;
        }
        if (it->second.gpuId != 0) DeleteTexture(it->second.gpuId);
    }
    CachedAssetTexture cached;
    cached.width = width;
    cached.height = height;
    cached.rgba8 = rgba8;
    cached.repeatWrap = repeatWrap;
    cached.generateMips = generateMips;
    cached.gpuId = UploadRGBA8Texture(width, height, cached.rgba8.data(), repeatWrap, generateMips);
    if (cached.gpuId == 0) return false;
    m_assetTextures[key] = std::move(cached);
    return true;
}

bool MobileRenderer::LoadWorldTexture(const std::string& key, uint32_t width, uint32_t height,
                                      const std::vector<uint8_t>& rgba8) {
    const bool mips = (m_settings.textureQuality >= 1);
    return LoadAssetTexture(key, width, height, rgba8, true, mips);
}

uint32_t MobileRenderer::GetAssetTexture(const std::string& key) const {
    auto it = m_assetTextures.find(key);
    return it == m_assetTextures.end() ? 0 : it->second.gpuId;
}

bool MobileRenderer::HasAssetTexture(const std::string& key) const {
    return GetAssetTexture(key) != 0;
}

void MobileRenderer::UnloadAssetTexture(const std::string& key) {
    auto it = m_assetTextures.find(key);
    if (it == m_assetTextures.end()) return;
    if (it->second.gpuId != 0) DeleteTexture(it->second.gpuId);
    m_assetTextures.erase(it);
}

void MobileRenderer::DrawTexturedQuad(float x, float y, float width, float height,
                                      uint32_t texId, float r, float g, float b, float a,
                                      float alphaCutoff) {
    if (texId == 0) return;
    m_stats.drawCalls++;
    m_stats.triangles += 2;
#ifdef __ANDROID__
    if (!HasGPUContext()) return;
    glUseProgram(m_uiProgram);
    glUniformMatrix4fv(m_uProjLoc, 1, GL_FALSE, m_projection);
    glUniform4f(m_uColorLoc, r, g, b, a);
    glUniform1i(m_uUseTexLoc, 2);
    glUniform1f(m_uAlphaCutoffLoc, alphaCutoff);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texId));
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
    glBindTexture(GL_TEXTURE_2D, 0);
#else
    (void)x; (void)y; (void)width; (void)height; (void)r; (void)g; (void)b; (void)a;
    (void)alphaCutoff;
#endif
}

uint32_t MobileRenderer::RegisterMesh3D(const std::vector<EP7WorldVertex>& vertices,
                                          const std::vector<uint16_t>& indices) {
    if (vertices.empty() || indices.empty()) return 0;
    const uint32_t handle = m_nextMeshHandle++;
    CachedMesh3D mesh;
    mesh.handle = handle;
    mesh.vertices = vertices;
    mesh.indices = indices;
    if (HasGPUContext()) {
        UploadMesh3DGPU(mesh);
    }
    m_meshes3D.emplace(handle, std::move(mesh));
    return handle;
}

void MobileRenderer::UnregisterMesh3D(uint32_t meshHandle) {
    auto it = m_meshes3D.find(meshHandle);
    if (it == m_meshes3D.end()) return;
#ifdef __ANDROID__
    if (HasGPUContext()) {
        if (it->second.vao) { GLuint v = it->second.vao; glDeleteVertexArrays(1, &v); }
        if (it->second.vbo) { GLuint b = it->second.vbo; glDeleteBuffers(1, &b); }
        if (it->second.ibo) { GLuint b = it->second.ibo; glDeleteBuffers(1, &b); }
    }
#endif
    m_meshes3D.erase(it);
}

void MobileRenderer::ClearMeshes3D() {
#ifdef __ANDROID__
    if (HasGPUContext()) {
        for (auto& entry : m_meshes3D) {
            if (entry.second.vao) { GLuint v = entry.second.vao; glDeleteVertexArrays(1, &v); }
            if (entry.second.vbo) { GLuint b = entry.second.vbo; glDeleteBuffers(1, &b); }
            if (entry.second.ibo) { GLuint b = entry.second.ibo; glDeleteBuffers(1, &b); }
        }
    }
#endif
    m_meshes3D.clear();
}

bool MobileRenderer::BeginWorldPass(const MobileCamera& camera, const WorldLightingState& lighting) {
    if (!HasGPUContext()) return false;
    SetCameraMatrices(camera);
    m_worldPassActive = true;
    m_lastBoundWorldTex = 0xFFFFFFFFu;

#ifdef __ANDROID__
    if (m_worldProgram == 0) return false;
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glFrontFace(GL_CW); // EP7 D3D9 meshes use CW front-face winding in LH space
    glCullFace(GL_BACK);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    glUseProgram(m_worldProgram);
    glUniformMatrix4fv(m_wViewProjLoc, 1, GL_FALSE, m_viewProjMatrix);
    glUniform3f(m_wCameraPosLoc, m_cameraPos[0], m_cameraPos[1], m_cameraPos[2]);
    glUniform1i(m_wTexLoc, 0);
    glUniform3f(m_wAmbientLoc, lighting.ambientColor.r, lighting.ambientColor.g, lighting.ambientColor.b);
    glUniform3f(m_wDirDirLoc, lighting.dirLightDir.x, lighting.dirLightDir.y, lighting.dirLightDir.z);
    glUniform3f(m_wDirColorLoc, lighting.dirLightColor.r, lighting.dirLightColor.g, lighting.dirLightColor.b);

    const int plCount = static_cast<int>(std::min<uint32_t>(lighting.pointLightCount, 4u));
    glUniform1i(m_wPointCountLoc, plCount);
    float posInvR[16]{};
    float pCol[12]{};
    for (int i = 0; i < plCount; ++i) {
        const auto& pl = lighting.pointLights[static_cast<size_t>(i)];
        posInvR[i * 4 + 0] = pl.x;
        posInvR[i * 4 + 1] = pl.y;
        posInvR[i * 4 + 2] = pl.z;
        posInvR[i * 4 + 3] = (pl.range > 1e-3f) ? (1.0f / pl.range) : 0.01f;
        pCol[i * 3 + 0] = pl.r;
        pCol[i * 3 + 1] = pl.g;
        pCol[i * 3 + 2] = pl.b;
    }
    glUniform4fv(m_wPointPosInvRangeLoc, 4, posInvR);
    glUniform3fv(m_wPointColorLoc, 4, pCol);

    glUniform1i(m_wFogEnabledLoc, lighting.fog.enabled ? 1 : 0);
    glUniform1f(m_wFogStartLoc, lighting.fog.fogStart);
    glUniform1f(m_wFogEndLoc, lighting.fog.fogEnd);
    const EP7Color4& fc = lighting.fog.fogColor;
    glUniform3f(m_wFogColorLoc, fc.r, fc.g, fc.b);
#else
    (void)lighting;
#endif
    return true;
}

void MobileRenderer::DrawMesh3D(uint32_t meshHandle,
                                uint32_t texId,
                                const EP7Mat4& modelMatrix,
                                const EP7AABB& worldBounds,
                                EP7AlphaBucket bucket,
                                bool twoSided,
                                float alphaCutoff,
                                const EP7Color4& tint,
                                bool depthWrite,
                                bool unlit,
                                bool skipCulling) {
    if (!m_worldPassActive || meshHandle == 0) return;
    auto it = m_meshes3D.find(meshHandle);
    if (it == m_meshes3D.end()) return;
    const CachedMesh3D& mesh = it->second;
    if (mesh.indices.empty()) return;

    if (!skipCulling && worldBounds.valid) {
        if (!IsBoxInFrustum(worldBounds.min.x, worldBounds.min.y, worldBounds.min.z,
                            worldBounds.max.x, worldBounds.max.y, worldBounds.max.z)) {
            m_stats.culledObjects++;
            return;
        }
    }

    m_stats.drawCalls++;
    m_stats.triangles += static_cast<uint32_t>(mesh.indices.size() / 3u);

#ifdef __ANDROID__
    if (!HasGPUContext() || m_worldProgram == 0 || mesh.vao == 0) return;
    if (twoSided) {
        glDisable(GL_CULL_FACE);
    } else {
        glEnable(GL_CULL_FACE);
    }
    glDepthMask(depthWrite ? GL_TRUE : GL_FALSE);

    const bool softBlend = (bucket == EP7AlphaBucket::SoftAlpha ||
                            bucket == EP7AlphaBucket::SoftAlpha01 ||
                            bucket == EP7AlphaBucket::SoftAlpha02);
    if (softBlend) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }

    float colModel[16]{};
    TransposeD3DToGL(modelMatrix, colModel);
    glUniformMatrix4fv(m_wModelLoc, 1, GL_FALSE, colModel);
    glUniform1f(m_wAlphaCutoffLoc, alphaCutoff);
    glUniform4f(m_wTintLoc, tint.r, tint.g, tint.b, tint.a);
    glUniform1i(m_wUnlitLoc, unlit ? 1 : 0);

    if (texId != m_lastBoundWorldTex) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texId));
        m_lastBoundWorldTex = texId;
    }
    glUniform1i(m_wHasTexLoc, texId != 0 ? 1 : 0);

    glBindVertexArray(static_cast<GLuint>(mesh.vao));
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.indices.size()), GL_UNSIGNED_SHORT, nullptr);
    glBindVertexArray(0);
#else
    (void)texId; (void)modelMatrix; (void)bucket; (void)twoSided;
    (void)alphaCutoff; (void)tint; (void)depthWrite; (void)unlit;
#endif
}

void MobileRenderer::EndWorldPass() {
    if (!m_worldPassActive) return;
    m_worldPassActive = false;
    m_lastBoundWorldTex = 0xFFFFFFFFu;
#ifdef __ANDROID__
    if (HasGPUContext()) {
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDisable(GL_CULL_FACE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
#endif
}

} // namespace RANMobile
