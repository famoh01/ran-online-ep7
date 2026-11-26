#include "MobileRenderer.h"
#include "../Core/Logger.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <fstream>
#include <cstring>

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
    // Map virtual design coordinates to the physical surface without stretching.
    // The matrix is rebuilt only on resize, never per draw call.
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

bool MobileRenderer::CreateGPUResources() {
#ifdef __ANDROID__
    GLuint vs = CompileShader(GL_VERTEX_SHADER, kUIVertexShader);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kUIFragmentShader);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        Logger::Error("MobileRenderer", "UI shader compilation failed; rendering disabled for this surface");
        return false;
    }
    m_uiProgram = glCreateProgram();
    glAttachShader(m_uiProgram, vs);
    glAttachShader(m_uiProgram, fs);
    glLinkProgram(m_uiProgram);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint linked = 0;
    glGetProgramiv(m_uiProgram, GL_LINK_STATUS, &linked);
    if (!linked) {
        char log[512];
        glGetProgramInfoLog(m_uiProgram, sizeof(log), nullptr, log);
        Logger::Error("MobileRenderer", std::string("UI program link failed: ") + log);
        glDeleteProgram(m_uiProgram);
        m_uiProgram = 0;
        return false;
    }
    m_uProjLoc = glGetUniformLocation(m_uiProgram, "uProj");
    m_uColorLoc = glGetUniformLocation(m_uiProgram, "uColor");
    m_uUseTexLoc = glGetUniformLocation(m_uiProgram, "uUseTexture");

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
    glClearColor(0.03f, 0.06f, 0.13f, 1.0f);
    CheckGLError("CreateGPUResources");
#endif
    m_gpuReady = true;
    return true;
}

void MobileRenderer::DestroyGPUResources(bool deleteObjects) {
#ifdef __ANDROID__
    if (deleteObjects) {
        for (uint32_t tex : m_ownedTextures) {
            GLuint id = static_cast<GLuint>(tex);
            glDeleteTextures(1, &id);
        }
        if (m_fontAtlasTex) glDeleteTextures(1, &m_fontAtlasTex);
        if (m_uiVBO) glDeleteBuffers(1, &m_uiVBO);
        if (m_uiVAO) glDeleteVertexArrays(1, &m_uiVAO);
        if (m_uiProgram) glDeleteProgram(m_uiProgram);
    }
    // After context loss the old handles are invalid; drop them without GL calls.
    m_fontAtlasTex = 0;
    m_uiVBO = 0;
    m_uiVAO = 0;
    m_uiProgram = 0;
    m_uProjLoc = -1;
    m_uColorLoc = -1;
    m_uUseTexLoc = -1;
#else
    (void)deleteObjects;
#endif
    m_ownedTextures.clear();
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

    if (!CreateGPUResources()) {
        Logger::Error("MobileRenderer", "Renderer initialization failed at GPU resource creation");
        return false;
    }

    m_initialized = true;
    Logger::Info("MobileRenderer", "Initialized GLES3 UI renderer, physical " +
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
    // A new EGL surface means all previous GL handles are invalid.
    DestroyGPUResources(false);
    if (!CreateGPUResources()) {
        Logger::Error("MobileRenderer", "Surface recreation failed; frame rendering suspended");
        return;
    }
    Logger::Info("MobileRenderer", "Surface recreated " +
                 std::to_string(width) + "x" + std::to_string(height) + "; glyph atlas reset");
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
    m_hasPrimaryFont = false;
    m_hasThaiFont = false;
    m_initialized = false;
    Logger::Info("MobileRenderer", "Renderer shut down");
}

bool MobileRenderer::BeginFrame() {
    m_stats = RenderStats{};
#ifdef __ANDROID__
    if (!m_initialized || !HasGPUContext()) return false;
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    CheckGLError("BeginFrame/glClear");
#endif
    return HasGPUContext();
}

void MobileRenderer::EndFrame() {
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
    if (!HasGPUContext()) return 0;
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
    CheckGLError("UploadRGBA8Texture");
    m_ownedTextures.push_back(static_cast<uint32_t>(texId));
    return static_cast<uint32_t>(texId);
#else
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

} // namespace RANMobile
