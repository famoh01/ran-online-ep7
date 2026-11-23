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
#endif
    m_glyphCache.clear();
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

} // namespace RANMobile
