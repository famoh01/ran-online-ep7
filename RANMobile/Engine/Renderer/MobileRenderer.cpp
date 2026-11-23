#include "MobileRenderer.h"
#include "../Core/Logger.h"

#ifdef __ANDROID__
#include <GLES3/gl3.h>
#endif

namespace RANMobile {

bool MobileRenderer::Initialize(int width, int height) {
    m_width = width;
    m_height = height;
    m_settings.ApplyPreset(GraphicsQuality::Medium);

#ifdef __ANDROID__
    glViewport(0, 0, m_width, m_height);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glClearColor(0.05f, 0.07f, 0.12f, 1.0f);
#endif

    m_initialized = true;
    Logger::Info("MobileRenderer", "Initialized OpenGL ES 3.0 / Portable Renderer (" +
                 std::to_string(m_width) + "x" + std::to_string(m_height) + ")");
    return true;
}

void MobileRenderer::Resize(int width, int height) {
    m_width = width;
    m_height = height;
#ifdef __ANDROID__
    if (m_initialized) {
        glViewport(0, 0, m_width, m_height);
    }
#endif
}

void MobileRenderer::Shutdown() {
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
    // Swap buffers handled by EGL/NativeActivity on Android
}

void MobileRenderer::DrawUIQuad(float x, float y, float width, float height, float r, float g, float b, float a) {
    m_stats.drawCalls++;
    m_stats.triangles += 2;
#ifdef __ANDROID__
    if (!m_initialized) return;
    glEnable(GL_SCISSOR_TEST);
    glScissor(static_cast<GLint>(x), static_cast<GLint>(m_height - y - height),
              static_cast<GLsizei>(width), static_cast<GLsizei>(height));
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
#else
    (void)x; (void)y; (void)width; (void)height; (void)r; (void)g; (void)b; (void)a;
#endif
}

void MobileRenderer::DrawUIText(float x, float y, const std::string& text, float scale, float r, float g, float b, float a) {
    if (text.empty()) return;
    m_stats.drawCalls += static_cast<uint32_t>(text.size());
    m_stats.triangles += static_cast<uint32_t>(text.size() * 2);
    (void)x; (void)y; (void)scale; (void)r; (void)g; (void)b; (void)a;
}

void MobileRenderer::ApplySettings(const GraphicsSettings& settings) {
    m_settings = settings;
    Logger::Info("MobileRenderer", "Applied graphics settings: FPS=" +
                 std::to_string(m_settings.fpsLimit) +
                 ", ViewDist=" + std::to_string(static_cast<int>(m_settings.viewDistance)));
}

bool MobileRenderer::IsBoxInFrustum(float minX, float minY, float minZ, float maxX, float maxY, float maxZ) const {
    (void)minY;
    (void)maxY;
    // Basic distance/bounds check against viewDistance
    float centerX = (minX + maxX) * 0.5f;
    float centerZ = (minZ + maxZ) * 0.5f;
    float distSq = centerX * centerX + centerZ * centerZ;
    return distSq <= (m_settings.viewDistance * m_settings.viewDistance);
}

} // namespace RANMobile
