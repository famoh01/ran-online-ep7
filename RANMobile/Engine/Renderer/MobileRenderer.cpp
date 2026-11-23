#include "MobileRenderer.h"
#include "../Core/Logger.h"

#ifdef __ANDROID__
#include <GLES3/gl3.h>
#endif

namespace RANMobile {

#ifdef __ANDROID__
static const char* kUIVertexShader = R"(#version 300 es
layout(location = 0) in vec2 aPos;
uniform mat4 uProj;
void main() {
    gl_Position = uProj * vec4(aPos, 0.0, 1.0);
}
)";

static const char* kUIFragmentShader = R"(#version 300 es
precision mediump float;
uniform vec4 uColor;
out vec4 FragColor;
void main() {
    FragColor = uColor;
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

// Compact 5x7 bitmap font for printable ASCII (32..126) - 5 bytes per glyph (columns)
static const uint8_t* GetGlyph5x7(char c) {
    static const uint8_t space[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
    static const uint8_t star[5]  = {0x14, 0x08, 0x3E, 0x08, 0x14};
    static const uint8_t colon[5] = {0x00, 0x36, 0x36, 0x00, 0x00};
    static const uint8_t dash[5]  = {0x08, 0x08, 0x08, 0x08, 0x08};
    static const uint8_t dot[5]   = {0x00, 0x60, 0x60, 0x00, 0x00};
    static const uint8_t fallback[5] = {0x7F, 0x41, 0x41, 0x41, 0x7F};
    if (c == ' ') return space;
    if (c == '*') return star;
    if (c == ':') return colon;
    if (c == '-') return dash;
    if (c == '.') return dot;
    return fallback;
}

bool MobileRenderer::Initialize(int width, int height) {
    m_width = width > 0 ? width : 1280;
    m_height = height > 0 ? height : 720;
    m_settings.ApplyPreset(GraphicsQuality::Medium);

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
    }

    glGenVertexArrays(1, &m_uiVAO);
    glGenBuffers(1, &m_uiVBO);
    glBindVertexArray(m_uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_uiVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 8, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glBindVertexArray(0);
#endif

    m_initialized = true;
    Logger::Info("MobileRenderer", "Initialized OpenGL ES 3.0 / Portable Renderer (" +
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
    if (m_uiVBO) { glDeleteBuffers(1, &m_uiVBO); m_uiVBO = 0; }
    if (m_uiVAO) { glDeleteVertexArrays(1, &m_uiVAO); m_uiVAO = 0; }
    if (m_uiProgram) { glDeleteProgram(m_uiProgram); m_uiProgram = 0; }
#endif
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

    float verts[8] = {
        x, y,
        x + width, y,
        x, y + height,
        x + width, y + height
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
    float pxSize = 2.5f * scale;
    float cursorX = x;
    for (char c : text) {
        const uint8_t* glyph = GetGlyph5x7(c);
        for (int col = 0; col < 5; ++col) {
            for (int row = 0; row < 7; ++row) {
                if (glyph[col] & (1 << row)) {
                    DrawUIQuad(cursorX + col * pxSize, y + row * pxSize, pxSize, pxSize, r, g, b, a);
                }
            }
        }
        cursorX += 6.0f * pxSize;
    }
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
