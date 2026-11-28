// ---------------------------------------------------------------------------
// B19: live-GLES3 GPU verification harness (see GpuSelfTest.h).
//
// SYNTHETIC fixtures used here (tiny quad meshes, 2x2 textures, 2-bone
// skeletons) are explicitly labeled SYNTHETIC and exist only to make the GPU
// math deterministic and comparable against the CPU reference.  The
// authentic-character section (when assets are provided) uses ONLY original
// EP7 client files supplied by the caller; it fabricates nothing.
// ---------------------------------------------------------------------------
#include "GpuSelfTest.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../../Data/Importer/EP7SkinnedMesh.h"
#include "../../Data/Importer/EP7Skeleton.h"
#include "../../Data/Importer/EP7SkinPiece.h"
#include "../../Data/Importer/EP7TextureDecoder.h"
#include "../Core/Logger.h"

#ifdef __ANDROID__
#include <android/asset_manager.h>
#include <GLES3/gl3.h>
#endif

namespace RANMobile {
namespace {

// JSON helpers ----------------------------------------------------------------
std::string JStr(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') {
            out.push_back('\\');
            out.push_back(c);
        } else if (c == '\n') {
            out += "\\n";
        } else {
            out.push_back(c);
        }
    }
    out.push_back('"');
    return out;
}

struct TestLog {
    bool pass = true;
    std::string entries;  // pre-rendered JSON objects, comma separated
    void Record(const std::string& name, bool ok, const std::string& detail) {
        if (!ok) pass = false;
        entries += (entries.empty() ? "" : ",\n        ");
        entries += "{\"name\": " + JStr(name) + ", \"pass\": " + (ok ? "true" : "false") +
                   ", \"detail\": " + JStr(detail) + "}";
    }
};

// SYNTHETIC fixtures (Android-only: the live-GLES path; headless builds skip them).
#ifdef __ANDROID__
// 2-bone chain (Bip01 -> Bip01_Spine1), the same shape the unit-test fixture uses.
std::string SkelX() {
    std::string s = "xof 0303txt 0032\n";
    s += "template Frame {\n  <3D82AB46-62DA-11cf-AB39-0020AF71E433>\n}\n";
    s += "Frame Bip01 {\n";
    s += "  FrameTransformMatrix {\n    1.0,0.0,0.0,0.0, 0.0,1.0,0.0,0.0, 0.0,0.0,1.0,0.0, 0.0,0.0,0.0,1.0;;\n  }\n";
    s += "  Frame Bip01_Spine1 {\n";
    s += "    FrameTransformMatrix {\n      1.0,0.0,0.0,0.0, 0.0,1.0,0.0,0.0, 0.0,0.0,1.0,0.0, 0.0,2.0,0.0,1.0;;\n    }\n";
    s += "  }\n";
    s += "}\n";
    return s;
}

// 4-vertex quad, two influences per vertex (bone0 + bone1), like the unit fixture.
EP7SkinnedMesh QuadMesh() {
    EP7SkinnedMesh mesh;
    mesh.valid = true;
    mesh.geometryDecoded = true;
    mesh.vertexCount = 4;
    mesh.vertexStride = 32;
    const float pos[4][3] = {{-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}};
    for (int v = 0; v < 4; ++v) {
        for (int k = 0; k < 3; ++k) mesh.positions.push_back(pos[v][k]);
        mesh.normals.push_back(0.0f);
        mesh.normals.push_back(0.0f);
        mesh.normals.push_back(1.0f);
        mesh.texcoords.push_back((pos[v][0] + 1.0f) * 0.5f);
        mesh.texcoords.push_back((pos[v][1] + 1.0f) * 0.5f);
    }
    mesh.faces = {0, 1, 2, 0, 2, 3};
    EP7SkinBoneInfo bone0;
    bone0.skeletonBoneIndex = 0;
    for (int v = 0; v < 4; ++v) {
        bone0.vertices.push_back(v);
        bone0.weights.push_back(0.75f);
    }
    EP7SkinBoneInfo bone1;
    bone1.skeletonBoneIndex = 1;
    for (int v = 0; v < 4; ++v) {
        bone1.vertices.push_back(v);
        bone1.weights.push_back(0.25f);
    }
    mesh.bones.push_back(std::move(bone0));
    mesh.bones.push_back(std::move(bone1));
    return mesh;
}

// Two-subset quad: subset 0 = left triangle pair, subset 1 = right (SYNTHETIC).
EP7SkinnedMesh TwoSubsetMesh() {
    EP7SkinnedMesh mesh;
    mesh.valid = true;
    mesh.geometryDecoded = true;
    // Two quads side by side: left (x in [-1,0], red subset 0) and right
    // (x in [0,1], green subset 1).  4 faces total: subset 0 = faces 0-1
    // (left), subset 1 = faces 2-3 (right).  The original fixture computed the
    // right quad then discarded it ((void)pos2) — B19 CI caught the right
    // subset sample drawing nothing (black).
    mesh.vertexCount = 8;
    mesh.vertexStride = 32;
    const float pos[8][3] = {{-1.0f, -1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f},
                             {0.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};
    for (int v = 0; v < 8; ++v) {
        for (int k = 0; k < 3; ++k) mesh.positions.push_back(pos[v][k]);
        mesh.normals.push_back(0.0f);
        mesh.normals.push_back(0.0f);
        mesh.normals.push_back(1.0f);
        mesh.texcoords.push_back((pos[v][0] + 1.0f) * 0.5f);
        mesh.texcoords.push_back((pos[v][1] + 1.0f) * 0.5f);
    }
    // Left quad CCW and right quad CCW in GL window space (y up).
    mesh.faces = {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7};
    EP7MeshSubset s0;
    s0.materialIndex = 0;
    s0.attributeId = 0;
    s0.faceStart = 0;
    s0.faceCount = 2;
    EP7MeshSubset s1;
    s1.materialIndex = 1;
    s1.attributeId = 1;
    s1.faceStart = 2;
    s1.faceCount = 2;
    mesh.subsets.push_back(s0);
    mesh.subsets.push_back(s1);
    mesh.textures = {"", ""};
    return mesh;
}

#endif  // __ANDROID__

}  // namespace

// Authentic-asset resolver shared by the loader, the scene texture ladder and the
// manifest scan.  Resolution order: filesystem root (CI adb-push), then the APK
// assets/ep7auth/ directory, each with a lowercase fallback (pack entry names are
// lowercase).
struct AuthCtx {
    std::string root;
    void* mgr;
};

bool AuthResolve(void* ud, const std::string& name, std::vector<uint8_t>& out) {
    auto lower = [](std::string s) {
        for (auto& c : s) {
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        }
        return s;
    };
    auto tryOpen = [&](const std::string& n) -> bool {
#ifdef __ANDROID__
        AAssetManager* mgr = static_cast<AAssetManager*>(static_cast<AuthCtx*>(ud)->mgr);
        if (mgr) {
            const std::string assetName = "ep7auth/" + n;
            AAsset* asset = AAssetManager_open(mgr, assetName.c_str(), AASSET_MODE_BUFFER);
            if (asset) {
                const size_t len = static_cast<size_t>(AAsset_getLength(asset));
                out.resize(len);
                std::memcpy(out.data(), AAsset_getBuffer(asset), len);
                AAsset_close(asset);
                return true;
            }
        }
#else
        (void)ud;
#endif
        const std::string& root = static_cast<AuthCtx*>(ud)->root;
        if (root.empty()) return false;
        std::FILE* f = std::fopen((root + "/" + n).c_str(), "rb");
        if (!f) return false;
        std::fseek(f, 0, SEEK_END);
        const long size = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        out.resize(static_cast<size_t>(size));
        const size_t got = size > 0 ? std::fread(out.data(), 1, out.size(), f) : 0;
        std::fclose(f);
        return got == out.size();
    };
    if (tryOpen(name)) return true;
    if (tryOpen(lower(name))) return true;
    return false;
}

GpuSelfTestResult RunGpuSelfTest(MobileRenderer& renderer, void* assetManager,
                                 const std::string& authenticAssetRoot,
                                 const std::string& screenshotOutPath) {
    GpuSelfTestResult result;
    std::string json;
    json.reserve(8192);

#ifndef __ANDROID__
    (void)renderer;
    (void)assetManager;
    (void)authenticAssetRoot;
    (void)screenshotOutPath;
    result.json =
        "{\"harness\": \"gpu_selftest\", \"environment\": \"headless\", "
        "\"attempted\": false, \"verdict\": \"SKIPPED\", "
        "\"note\": \"no GL context in headless builds; run on Android device/emulator\"}";
    return result;
#else
    result.attempted = true;
    json += "{\n    \"harness\": \"gpu_selftest\",\n    \"environment\": \"android_gles3\",\n";

    // ------------------------------------------------------------------ 1. env
    const char* glRenderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const char* glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const char* glSl = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));
    GLint maxBlock = 0;
    GLint maxTexSize = 0;
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &maxBlock);
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
    const std::string rendererStr = glRenderer ? glRenderer : "";
    const std::string versionStr = glVersion ? glVersion : "";
    const std::string slStr = glSl ? glSl : "";
    json += "    \"gl\": {\"renderer\": " + JStr(rendererStr) + ", \"version\": " + JStr(versionStr) +
            ", \"glsl\": " + JStr(slStr) + ", \"maxUniformBlockSize\": " + std::to_string(maxBlock) +
            ", \"maxTextureSize\": " + std::to_string(maxTexSize) + "},\n";
    Logger::Info("GpuSelfTest", "GL renderer: " + rendererStr + " / " + versionStr);

    TestLog tests;

    // ------------------------------------------- 2. shader compile/link (live)
    {
        // renderer.Initialize() already compiled+linked both programs against the
        // live driver (compile/link errors carry driver info logs).  The GPU
        // program's readiness is the compile/link evidence; the 16 KB uniform
        // block is the ES 3.0 guarantee the palette UBO relies on.
        const bool ready = renderer.GpuSkinProgramReady();
        // B19 CI: embed the driver's compile/link info log so the verdict JSON
        // (the only evidence channel readable off-box) carries the exact
        // GLSL error instead of a bare boolean.
        std::string driverLog = renderer.GpuSkinProgramLog();
        if (driverLog.size() > 400) driverLog = driverLog.substr(0, 400);
        tests.Record("shader_compile_link", ready,
                     ready ? "GPU skin program linked on live GL (renderer Initialize)"
                           : "GPU skin program failed to compile/link; driver: " +
                                 (driverLog.empty() ? std::string("<no info log>") : driverLog));
        tests.Record("palette_texture_transport", maxTexSize >= 1024,
                     "MAX_TEXTURE_SIZE=" + std::to_string(maxTexSize) +
                         " required>=1024 (1024x1 RGBA32F palette, B19 live-GLES correction)");
        const char* vs = MobileRenderer::CharGpuVertexShaderSource();
        const char* fs = MobileRenderer::CharGpuFragmentShaderSource();
        tests.Record("shader_source_available", vs != nullptr && fs != nullptr,
                     "production GLSL sources inspected by harness");
    }

    if (!renderer.IsInitialized()) {
        renderer.Initialize(64, 64);
    }

    // ------------------- 3. CPU vs GPU skinning (raster position probe)
    // B19 CI evidence (three iterations): transform-feedback capture is
    // unusable on the emulator's GLES translator.  The TF-session draw fails
    // with GL_INVALID_OPERATION (errDraw=1282 with clean begin/end) because
    // the production draw path's glBindVertexArray clobbers the
    // transform-feedback buffer binding on stacks that scope TF bindings to
    // the vertex array object; with RASTERIZER_DISCARD enabled the translator
    // captures nothing at all.  The probe transports the PRODUCTION vertex
    // shader's skinned outputs through the rasterization path instead (which
    // this stack demonstrably supports — the section-4 pixel tests pass):
    // the probe program reuses the production vertex shader VERBATIM (one
    // appended gl_PointSize statement), and its fragment shader encodes
    // vSkinnedPos/vNormal into 16-bit-split RGBA8 pixels (quantization
    // 1.2e-4 for positions in [-4,4] and 3.1e-5 for normals in [-1,1] — both
    // far below the 5e-4/5e-3 tolerances).  One GL_POINTS draw plus one 1x1
    // glReadPixels per probed vertex: bounded (<=48 probes x 3 passes),
    // deterministic, no transform feedback, no extensions.
    auto probeCompare = [&](const char* label, const EP7SkinnedMesh& mesh,
                            const std::vector<ep7math::Mat4>& palette) -> bool {
        const EP7GpuSkinStreams streams = BuildGpuSkinStreams(mesh);
        if (!streams.valid) { tests.Record(label, false, "stream build failed"); return false; }
        // Interleave exactly like EP7CharacterScene::EnsurePieceGpuHandle.
        std::vector<float> interleaved(static_cast<size_t>(mesh.vertexCount) * 16);
        for (uint32_t v = 0; v < mesh.vertexCount; ++v) {
            float* d = interleaved.data() + static_cast<size_t>(v) * 16;
            for (int k = 0; k < 3; ++k) {
                d[k] = mesh.positions[static_cast<size_t>(v) * 3 + k];
                d[3 + k] = mesh.normals[static_cast<size_t>(v) * 3 + k];
            }
            d[6] = mesh.texcoords[static_cast<size_t>(v) * 2];
            d[7] = mesh.texcoords[static_cast<size_t>(v) * 2 + 1];
            for (int k = 0; k < 4; ++k) {
                d[8 + k] = streams.boneIndices[static_cast<size_t>(v) * 4 + k];
                d[12 + k] = streams.boneWeights[static_cast<size_t>(v) * 4 + k];
            }
        }
        RANMobile::GpuSkinnedMeshData data;
        data.interleaved = interleaved.data();
        data.vertexCount = mesh.vertexCount;
        data.indices = mesh.faces.data();
        data.indexCount = static_cast<uint32_t>(mesh.faces.size());
        const uint32_t handle = renderer.UploadGpuSkinnedMesh(data);
        if (handle == 0) { tests.Record(label, false, "UploadGpuSkinnedMesh failed"); return false; }

        // CPU reference (SkinVerticesCPU — same palette semantics as B17).
        std::vector<float> cpuPos;
        std::vector<float> cpuNrm;
        if (!SkinVerticesCPU(mesh, palette, cpuPos, cpuNrm)) {
            renderer.ReleaseGpuSkinnedMesh(handle);
            tests.Record(label, false, "CPU reference skinning failed");
            return false;
        }

        // Upload the palette through the PRODUCTION draw path: DrawGpuSkinnedMesh
        // populates the shared palette texture and leaves it bound on texture
        // unit 1; the probe program samples that exact texture, so the palette
        // transport is validated verbatim (no second upload path).
        std::vector<float> paletteRow;
        paletteRow.reserve(palette.size() * 16);
        for (const auto& m : palette) {
            for (int r = 0; r < 4; ++r) {
                for (int c = 0; c < 4; ++c) paletteRow.push_back(m.m[r][c]);
            }
        }
        const float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        renderer.BeginScene3D(MobileCamera{});  // must precede draws (sets 3D pass state)
        const float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        renderer.SetViewProjectionOverride(identity);
        while (glGetError() != GL_NO_ERROR) {}
        const bool warmed = renderer.DrawGpuSkinnedMesh(handle, paletteRow.data(),
                                                        static_cast<uint32_t>(palette.size()), 0, white,
                                                        false, 0,
                                                        static_cast<uint32_t>(mesh.faces.size() / 3));
        const GLenum errWarm = glGetError();

        // Probe program: production vertex shader VERBATIM with one appended
        // gl_PointSize statement (points are not used by production rendering;
        // this keeps the validated skinning math bit-for-bit the production
        // shader's), plus an encoding fragment shader.
        static const char* kProbeFs =
            "#version 300 es\n"
            "precision highp float;\n"
            "in vec3 vSkinnedPos;\n"
            "in vec3 vNormal;\n"
            "uniform int uPass;\n"
            "uniform vec2 uRange0;\n"
            "uniform vec2 uRange1;\n"
            "out vec4 FragColor;\n"
            "vec2 enc16(float v, vec2 range) {\n"
            "    float t = clamp((v - range.x) / (range.y - range.x), 0.0, 1.0);\n"
            "    float q = floor(t * 65535.0 + 0.5);\n"
            "    return vec2(floor(q / 256.0) / 255.0, mod(q, 256.0) / 255.0);\n"
            "}\n"
            "void main() {\n"
            "    float a = uPass == 0 ? vSkinnedPos.x : (uPass == 1 ? vSkinnedPos.z : vNormal.y);\n"
            "    float b = uPass == 0 ? vSkinnedPos.y : (uPass == 1 ? vNormal.x : vNormal.z);\n"
            "    vec2 ea = enc16(a, uRange0);\n"
            "    vec2 eb = enc16(b, uRange1);\n"
            "    FragColor = vec4(ea.x, ea.y, eb.x, eb.y);\n"
            "}\n";
        std::string pvsSrc = MobileRenderer::CharGpuVertexShaderSource();
        const size_t lastBrace = pvsSrc.rfind('}');
        std::string probeLog;
        bool probeLinked = warmed && !pvsSrc.empty() && lastBrace != std::string::npos;
        GLuint probeProg = 0;
        if (probeLinked) {
            pvsSrc.insert(lastBrace, "    gl_PointSize = 1.0;\n");
            const char* vsPtr = pvsSrc.c_str();
            probeProg = glCreateProgram();
            GLuint vs = glCreateShader(GL_VERTEX_SHADER);
            glShaderSource(vs, 1, &vsPtr, nullptr);
            glCompileShader(vs);
            GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(fs, 1, &kProbeFs, nullptr);
            glCompileShader(fs);
            GLint vsOk = 0;
            glGetShaderiv(vs, GL_COMPILE_STATUS, &vsOk);
            GLint fsOk = 0;
            glGetShaderiv(fs, GL_COMPILE_STATUS, &fsOk);
            if (vsOk && fsOk) {
                glAttachShader(probeProg, vs);
                glAttachShader(probeProg, fs);
                glLinkProgram(probeProg);
                GLint linkOk = 0;
                glGetProgramiv(probeProg, GL_LINK_STATUS, &linkOk);
                if (!linkOk) {
                    char buf[320] = {0};
                    GLsizei n = 0;
                    glGetProgramInfoLog(probeProg, sizeof(buf) - 1, &n, buf);
                    probeLog = std::string("probe link: ") + buf;
                    probeLinked = false;
                }
            } else {
                char buf[320] = {0};
                GLsizei n = 0;
                if (!vsOk) {
                    glGetShaderInfoLog(vs, sizeof(buf) - 1, &n, buf);
                    probeLog = std::string("probe VS compile: ") + buf;
                } else {
                    glGetShaderInfoLog(fs, sizeof(buf) - 1, &n, buf);
                    probeLog = std::string("probe FS compile: ") + buf;
                }
                probeLinked = false;
            }
            glDeleteShader(vs);
            glDeleteShader(fs);
        }
        if (!probeLinked) {
            glDeleteProgram(probeProg);
            renderer.ReleaseGpuSkinnedMesh(handle);
            tests.Record(label, false,
                         probeLog.empty() ? "warmup draw failed; probe program not built"
                                          : probeLog);
            return false;
        }

        // Harness-side VAO over the SAME interleaved stream (the production VS
        // declares explicit layout(location = 0..4) attribute slots; stride 64
        // bytes: pos@0, normal@12, uv@24, boneIdx@32, boneWt@48).
        GLuint probeVbo = 0;
        GLuint probeVao = 0;
        glGenBuffers(1, &probeVbo);
        glBindBuffer(GL_ARRAY_BUFFER, probeVbo);
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(interleaved.size()) * sizeof(float),
                     interleaved.data(), GL_STATIC_DRAW);
        glGenVertexArrays(1, &probeVao);
        glBindVertexArray(probeVao);
        glBindBuffer(GL_ARRAY_BUFFER, probeVbo);
        const GLsizei kStride = 16 * static_cast<GLsizei>(sizeof(float));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, kStride, reinterpret_cast<const void*>(0));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, kStride, reinterpret_cast<const void*>(12));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, kStride, reinterpret_cast<const void*>(24));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, kStride, reinterpret_cast<const void*>(32));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, kStride, reinterpret_cast<const void*>(48));
        glBindVertexArray(0);

        // Probe render region: a 256x256 corner of the default EGL surface (the
        // rasterization path used by the passing pixel tests — no FBO, no
        // extensions).  Sentinel clear color = pure blue; a decoded value can
        // never read as blue (blue channel 255 means hi+lo are 0 -> range min
        // for BOTH components, which the CPU reference never produces for the
        // fixtures used here).
        const int W = 256;
        const int H = 256;
        renderer.Resize(W, H);
        glViewport(0, 0, W, H);
        const GLboolean depthWas = glIsEnabled(GL_DEPTH_TEST);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glDisable(GL_SCISSOR_TEST);
        glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(probeProg);
        glUniform1i(glGetUniformLocation(probeProg, "uBonePalette"), 1);
        // Probe VP: place skinned positions into the safe middle of the clip
        // volume (NDC x,y = 0.35*pos + 0.30, NDC z = 0.5*pos).  This VP affects
        // POINT PLACEMENT ONLY — the FS transports vSkinnedPos/vNormal (the
        // pre-VP skinned outputs), so the comparison is against the raw
        // production skinning results, not a projected variant.
        const float probeVP[16] = {0.35f, 0.0f, 0.0f, 0.0f,
                                   0.0f, 0.35f, 0.0f, 0.0f,
                                   0.0f, 0.0f, 0.5f, 0.0f,
                                   0.30f, 0.30f, 0.0f, 1.0f};  // column-major
        glUniformMatrix4fv(glGetUniformLocation(probeProg, "uVP"), 1, GL_FALSE, probeVP);
        const GLint passLoc = glGetUniformLocation(probeProg, "uPass");
        const GLint r0Loc = glGetUniformLocation(probeProg, "uRange0");
        const GLint r1Loc = glGetUniformLocation(probeProg, "uRange1");
        auto decode16 = [](uint8_t hi, uint8_t lo, float rMin, float rMax) {
            const float q = static_cast<float>(hi) * 256.0f + static_cast<float>(lo);
            return q / 65535.0f * (rMax - rMin) + rMin;
        };
        // Probed vertices: up to 48 spread across the mesh; vertices whose probe
        // point would land within 5% of the clip edge are skipped (documented).
        const uint32_t vertexCount = mesh.vertexCount;
        const uint32_t stride = vertexCount / 48 > 1 ? vertexCount / 48 : 1;
        float maxPosDiff = 0.0f;
        float maxNrmDiff = 0.0f;
        int probed = 0;
        GLenum errProbe = 0;
        GLenum errRead = 0;
        float firstA = 0.0f;
        float firstB = 0.0f;
        bool firstSampled = false;
        for (uint32_t v = 0; v < vertexCount && probed < 48; v += stride) {
            const float cx = cpuPos[static_cast<size_t>(v) * 3 + 0];
            const float cy = cpuPos[static_cast<size_t>(v) * 3 + 1];
            const float cz = cpuPos[static_cast<size_t>(v) * 3 + 2];
            const float ndcX = 0.35f * cx + 0.30f;
            const float ndcY = 0.35f * cy + 0.30f;
            const float ndcZ = 0.5f * cz;
            if (ndcX < -0.95f || ndcX > 0.95f || ndcY < -0.95f || ndcY > 0.95f ||
                ndcZ < -0.95f || ndcZ > 0.95f) {
                continue;
            }
            const int sx = static_cast<int>((ndcX + 1.0f) * 0.5f * static_cast<float>(W));
            const int sy = static_cast<int>((ndcY + 1.0f) * 0.5f * static_cast<float>(H));
            if (sx < 0 || sx >= W || sy < 0 || sy >= H) continue;
            for (int pass = 0; pass < 3; ++pass) {
                float rMin0 = -4.0f, rMax0 = 4.0f, rMin1 = -4.0f, rMax1 = 4.0f;
                if (pass == 1) { rMin1 = -1.0f; rMax1 = 1.0f; }
                else if (pass == 2) { rMin0 = -1.0f; rMax0 = 1.0f; rMin1 = -1.0f; rMax1 = 1.0f; }
                glUniform1i(passLoc, pass);
                glUniform2f(r0Loc, rMin0, rMax0);
                glUniform2f(r1Loc, rMin1, rMax1);
                glBindVertexArray(probeVao);
                glDrawArrays(GL_POINTS, static_cast<GLint>(v), 1);
                errProbe = glGetError();
                uint8_t c[4] = {0, 0, 0, 0};
                glReadPixels(sx, sy, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, c);
                errRead = glGetError();
                const float a = decode16(c[0], c[1], rMin0, rMax0);
                const float b = decode16(c[2], c[3], rMin1, rMax1);
                if (!firstSampled) {
                    firstA = a;
                    firstB = b;
                    firstSampled = true;
                }
                if (pass == 0) {
                    maxPosDiff = std::fmax(maxPosDiff, std::fabs(a - cx));
                    maxPosDiff = std::fmax(maxPosDiff, std::fabs(b - cy));
                } else if (pass == 1) {
                    maxPosDiff = std::fmax(maxPosDiff, std::fabs(a - cz));
                    maxNrmDiff = std::fmax(maxNrmDiff,
                                           std::fabs(b - cpuNrm[static_cast<size_t>(v) * 3 + 0]));
                } else {
                    maxNrmDiff = std::fmax(maxNrmDiff,
                                           std::fabs(a - cpuNrm[static_cast<size_t>(v) * 3 + 1]));
                    maxNrmDiff = std::fmax(maxNrmDiff,
                                           std::fabs(b - cpuNrm[static_cast<size_t>(v) * 3 + 2]));
                }
            }
            ++probed;
        }
        if (depthWas == GL_TRUE) glEnable(GL_DEPTH_TEST);
        glBindVertexArray(0);
        glDeleteVertexArrays(1, &probeVao);
        glDeleteBuffers(1, &probeVbo);
        glDeleteProgram(probeProg);
        renderer.ReleaseGpuSkinnedMesh(handle);

        const bool match = warmed && probed > 0 && errProbe == GL_NO_ERROR &&
                           errRead == GL_NO_ERROR &&
                           maxPosDiff <= 5e-4f && maxNrmDiff <= 5e-3f;
        char detail[224];
        std::snprintf(detail, sizeof(detail),
                      "maxPosDiff=%.2e maxNrmDiff=%.2e (tol 5e-4/5e-3) probed=%d/%u warm=%d "
                      "err=w%uPr%uRd%u first=(%.4f,%.4f)%s",
                      maxPosDiff, maxNrmDiff, probed, static_cast<unsigned>(vertexCount),
                      static_cast<int>(warmed), static_cast<unsigned>(errWarm),
                      static_cast<unsigned>(errProbe), static_cast<unsigned>(errRead),
                      firstA, firstB, probeLinked ? "" : probeLog.c_str());
        tests.Record(label, match, detail);
        return match;
    };

    {
        // Bind pose (identity combined) and an animated pose (Head-like 90 deg
        // rotation on bone 1, composed through BuildBonePalette — offset*combined).
        EP7Skeleton skeleton;
        const std::string sx = SkelX();
        if (!skeleton.LoadFromXFile(reinterpret_cast<const uint8_t*>(sx.data()), sx.size())) {
            tests.Record("cpu_gpu_skinning_bindpose", false, "skeleton fixture failed to load");
        } else {
            const EP7SkinnedMesh mesh = QuadMesh();

            skeleton.ResetToRestPose();
            std::vector<ep7math::Mat4> bindPalette;
            BuildBonePalette(mesh, skeleton, bindPalette);
            probeCompare("cpu_gpu_skinning_bindpose", mesh, bindPalette);

            // Animated pose: rotate the child bone 90 deg about Y (manual SQT),
            // then evaluate and compose the palette exactly like the runtime.
            EP7Bone& spine = const_cast<EP7Bone&>(skeleton.GetBone(1));
            spine.rotation = ep7math::Quat(0.0f, 0.70710678f, 0.0f, 0.70710678f);
            skeleton.UpdateBones(ep7math::Mat4(), 1.0f);
            std::vector<ep7math::Mat4> animPalette;
            BuildBonePalette(mesh, skeleton, animPalette);
            probeCompare("cpu_gpu_skinning_animated", mesh, animPalette);
        }
    }

    // ------------------------------------------- 4. pixel-level rasterization
    {
        const int W = 64;
        const int H = 64;
        renderer.Resize(W, H);
        glViewport(0, 0, W, H);
        const float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        renderer.BeginScene3D(MobileCamera{});
        renderer.SetViewProjectionOverride(identity);
        // Fragment lighting: n=(0,0,1), l=normalize(0,1,-1) -> n.l<0 -> lit=0.35.
        const float lit = 0.35f;
        auto toByte = [](float c) { return static_cast<int>(std::lround(c * 255.0f)); };
        auto readCenter = [&](int x, int y, uint8_t out[4]) {
            uint8_t px[4];
            glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
            for (int k = 0; k < 4; ++k) out[k] = px[k];
        };
        auto checkColor = [&](const char* what, const uint8_t got[4], float er, float eg, float eb,
                              float ea, int tol) {
            const int dr = got[0] - toByte(er);
            const int dg = got[1] - toByte(eg);
            const int db = got[2] - toByte(eb);
            const int da = got[3] - toByte(ea);
            const bool ok = std::fabs(dr) <= tol && std::fabs(dg) <= tol && std::fabs(db) <= tol &&
                            std::fabs(da) <= tol;
            char d[192];
            std::snprintf(d, sizeof(d), "got=(%u,%u,%u,%u) expected=(%d,%d,%d,%d) tol=%d", got[0],
                          got[1], got[2], got[3], toByte(er), toByte(eg), toByte(eb), toByte(ea), tol);
            tests.Record(what, ok, d);
        };

        // 4a. flat colour, no texture (also covers zero-weight passthrough: the
        // quad uses weights 0 via an empty-influence mesh? No — QuadMesh has
        // influences; for the passthrough variant use a mesh whose streams have
        // zero weights — BuildGpuSkinStreams keeps no-influence verts at w=0).
        EP7SkinnedMesh passMesh = QuadMesh();
        for (EP7SkinBoneInfo& bone : passMesh.bones) bone.vertices.clear(), bone.weights.clear();
        {
            const EP7GpuSkinStreams streams = BuildGpuSkinStreams(passMesh);
            RANMobile::GpuSkinnedMeshData data;
            std::vector<float> interleaved(static_cast<size_t>(passMesh.vertexCount) * 16, 0.0f);
            if (streams.valid) {
                for (uint32_t v = 0; v < passMesh.vertexCount; ++v) {
                    float* d = interleaved.data() + static_cast<size_t>(v) * 16;
                    for (int k = 0; k < 3; ++k) {
                        d[k] = passMesh.positions[static_cast<size_t>(v) * 3 + k];
                        d[3 + k] = passMesh.normals[static_cast<size_t>(v) * 3 + k];
                    }
                    d[6] = passMesh.texcoords[static_cast<size_t>(v) * 2];
                    d[7] = passMesh.texcoords[static_cast<size_t>(v) * 2 + 1];
                    for (int k = 0; k < 4; ++k) {
                        d[8 + k] = streams.boneIndices[static_cast<size_t>(v) * 4 + k];
                        d[12 + k] = streams.boneWeights[static_cast<size_t>(v) * 4 + k];
                    }
                }
            }
            data.interleaved = interleaved.data();
            data.vertexCount = passMesh.vertexCount;
            data.indices = passMesh.faces.data();
            data.indexCount = static_cast<uint32_t>(passMesh.faces.size());
            const uint32_t handle = renderer.UploadGpuSkinnedMesh(data);
            std::vector<ep7math::Mat4> identityPalette(2, ep7math::Mat4());
            std::vector<float> paletteRow(32, 0.0f);
            for (int m = 0; m < 2; ++m) {
                for (int r = 0; r < 4; ++r) {
                    for (int c = 0; c < 4; ++c) {
                        paletteRow[static_cast<size_t>(m) * 16 + static_cast<size_t>(r) * 4 + c] =
                            identityPalette[m].m[r][c];
                    }
                }
            }
            const float flat[4] = {1.0f, 0.0f, 0.0f, 1.0f};
            glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            renderer.DrawGpuSkinnedMesh(handle, paletteRow.data(), 2, 0, flat, false, 0, 2);
            uint8_t px[4];
            readCenter(W / 2, H / 2, px);
            checkColor("raster_flat_no_texture", px, flat[0] * lit, 0.0f, 0.0f, 1.0f, 6);
            renderer.ReleaseGpuSkinnedMesh(handle);
        }

        // 4b. textured draw (2x2 RGBA texture: red/green/blue/white, SYNTHETIC).
        {
            const uint8_t tex2x2[16] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
            const uint32_t texId = renderer.UploadRGBA8Texture(2, 2, tex2x2);
            EP7SkinnedMesh mesh = QuadMesh();
            const EP7GpuSkinStreams streams = BuildGpuSkinStreams(mesh);
            RANMobile::GpuSkinnedMeshData data;
            std::vector<float> interleaved(static_cast<size_t>(mesh.vertexCount) * 16, 0.0f);
            for (uint32_t v = 0; v < mesh.vertexCount; ++v) {
                float* d = interleaved.data() + static_cast<size_t>(v) * 16;
                for (int k = 0; k < 3; ++k) {
                    d[k] = mesh.positions[static_cast<size_t>(v) * 3 + k];
                    d[3 + k] = mesh.normals[static_cast<size_t>(v) * 3 + k];
                }
                d[6] = mesh.texcoords[static_cast<size_t>(v) * 2];
                d[7] = mesh.texcoords[static_cast<size_t>(v) * 2 + 1];
                for (int k = 0; k < 4; ++k) {
                    d[8 + k] = streams.boneIndices[static_cast<size_t>(v) * 4 + k];
                    d[12 + k] = streams.boneWeights[static_cast<size_t>(v) * 4 + k];
                }
            }
            data.interleaved = interleaved.data();
            data.vertexCount = mesh.vertexCount;
            data.indices = mesh.faces.data();
            data.indexCount = static_cast<uint32_t>(mesh.faces.size());
            const uint32_t handle = renderer.UploadGpuSkinnedMesh(data);
            std::vector<float> paletteRow(32, 0.0f);
            // Identity palette (combined=identity, offset=identity).
            for (int m = 0; m < 2; ++m) {
                for (int r = 0; r < 4; ++r) {
                    paletteRow[static_cast<size_t>(m) * 16 + static_cast<size_t>(r) * 4 + r] = 1.0f;
                }
            }
            const float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
            glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            // Center UV (0.5,0.5) samples the 2x2 texture bilinearly: average of all
            // four texels = (0.5, 0.5, 0.5, 1).
            renderer.DrawGpuSkinnedMesh(handle, paletteRow.data(), 2, texId, white, false, 0, 2);
            uint8_t px[4];
            readCenter(W / 2, H / 2, px);
            checkColor("raster_textured_bilinear_center", px, 0.5f * lit, 0.5f * lit, 0.5f * lit,
                       1.0f, 10);
            // Texel center UV (0.25,0.25) = pure red texel (bottom-left in GL space).
            readCenter(W / 4, H / 4, px);
            checkColor("raster_textured_texel_red", px, 1.0f * lit, 0.0f, 0.0f, 1.0f, 6);

            // 4c. alpha blending: same quad, alpha 0.5 red over cleared blue.
            glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            const float alphaRed[4] = {1.0f, 0.0f, 0.0f, 0.5f};
            renderer.DrawGpuSkinnedMesh(handle, paletteRow.data(), 2, 0, alphaRed, true, 0, 2);
            readCenter(W / 2, H / 2, px);
            // Blend math: dst' = src*srcA + dst*(1-srcA).  src=(lit,0,0,0.5),
            // dst=(0,0,1): R=0.5*lit, B=0.5*1.0.  (The prior expectation of
            // 0.5*(1-lit)+0.5 for blue had no shader/source basis — B19 CI
            // evidence: the GPU returned exactly the correct 0.5 blend.)
            checkColor("raster_alpha_blend", px, 0.5f * lit, 0.0f, 0.5f * 1.0f,
                       1.0f, 10);

            // 4d. subset draw offsets: 2-subset mesh, red left / green right.
            renderer.ReleaseGpuSkinnedMesh(handle);
            EP7SkinnedMesh ss = TwoSubsetMesh();
            EP7GpuSkinStreams ssStreams;
            ssStreams.valid = true;
            ssStreams.vertexCount = ss.vertexCount;
            ssStreams.boneIndices.assign(static_cast<size_t>(ss.vertexCount) * 4, 0.0f);
            ssStreams.boneWeights.assign(static_cast<size_t>(ss.vertexCount) * 4, 0.0f);
            RANMobile::GpuSkinnedMeshData ssData;
            std::vector<float> ssInter(static_cast<size_t>(ss.vertexCount) * 16, 0.0f);
            for (uint32_t v = 0; v < ss.vertexCount; ++v) {
                float* d = ssInter.data() + static_cast<size_t>(v) * 16;
                for (int k = 0; k < 3; ++k) {
                    d[k] = ss.positions[static_cast<size_t>(v) * 3 + k];
                    d[3 + k] = ss.normals[static_cast<size_t>(v) * 3 + k];
                }
                d[6] = ss.texcoords[static_cast<size_t>(v) * 2];
                d[7] = ss.texcoords[static_cast<size_t>(v) * 2 + 1];
            }
            ssData.interleaved = ssInter.data();
            ssData.vertexCount = ss.vertexCount;
            ssData.indices = ss.faces.data();
            ssData.indexCount = static_cast<uint32_t>(ss.faces.size());
            const uint32_t ssHandle = renderer.UploadGpuSkinnedMesh(ssData);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            const float red[4] = {1.0f, 0.0f, 0.0f, 1.0f};
            const float green[4] = {0.0f, 1.0f, 0.0f, 1.0f};
            renderer.DrawGpuSkinnedMesh(ssHandle, paletteRow.data(), 2, 0, red, false, 0, 2);
            renderer.DrawGpuSkinnedMesh(ssHandle, paletteRow.data(), 2, 0, green, false, 2, 2);
            uint8_t pxl[4];
            readCenter(W / 4, H / 2, pxl);
            checkColor("raster_subset_offset_left", pxl, lit, 0.0f, 0.0f, 1.0f, 6);
            uint8_t pxr[4];
            readCenter(W * 3 / 4, H / 2, pxr);
            checkColor("raster_subset_offset_right", pxr, 0.0f, lit, 0.0f, 1.0f, 6);
            renderer.ReleaseGpuSkinnedMesh(ssHandle);
            renderer.DeleteTexture(texId);
        }
    }

    // ------------------------------------------------ 5. GL resource lifecycle
    {
        EP7SkinnedMesh mesh = QuadMesh();
        const EP7GpuSkinStreams streams = BuildGpuSkinStreams(mesh);
        RANMobile::GpuSkinnedMeshData data;
        std::vector<float> interleaved(static_cast<size_t>(mesh.vertexCount) * 16, 0.0f);
        for (uint32_t v = 0; v < mesh.vertexCount; ++v) {
            float* d = interleaved.data() + static_cast<size_t>(v) * 16;
            for (int k = 0; k < 3; ++k) {
                d[k] = mesh.positions[static_cast<size_t>(v) * 3 + k];
                d[3 + k] = mesh.normals[static_cast<size_t>(v) * 3 + k];
            }
        }
        data.interleaved = interleaved.data();
        data.vertexCount = mesh.vertexCount;
        data.indices = mesh.faces.data();
        data.indexCount = static_cast<uint32_t>(mesh.faces.size());
        const uint32_t handle = renderer.UploadGpuSkinnedMesh(data);
        std::vector<float> paletteRow(32, 0.0f);
        const float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        renderer.BeginScene3D(MobileCamera{});
        const char* fail = nullptr;
        if (handle == 0 || renderer.GpuMeshCount() != 1) fail = "upload/count";
        // Invalid handle must fail loudly (never silently).
        if (!fail && renderer.DrawGpuSkinnedMesh(handle + 999, paletteRow.data(), 2, 0, white, false, 0, 2))
            fail = "invalid handle accepted";
        // Palette overflow must fail (never approximate).
        if (!fail && renderer.DrawGpuSkinnedMesh(handle, paletteRow.data(), 257, 0, white, false, 0, 2))
            fail = "palette 257 accepted";
        // Face-range OOB must fail.
        if (!fail && renderer.DrawGpuSkinnedMesh(handle, paletteRow.data(), 2, 0, white, false, 2, 2))
            fail = "face-range OOB accepted";
        // Double release must be safe: the second release is a no-op — the
        // count must STAY at the post-first-release value (the original
        // assertion expected another decrement, contradicting the safe no-op
        // requirement it was testing; B19 CI caught it).
        renderer.ReleaseGpuSkinnedMesh(handle);
        const uint32_t before = renderer.GpuMeshCount();
        renderer.ReleaseGpuSkinnedMesh(handle);
        if (!fail && renderer.GpuMeshCount() != before) fail = "double release not a no-op";
        if (!fail && renderer.GpuMeshCount() != 0) fail = "mesh count not zero";
        const bool ok = fail == nullptr;
        tests.Record("gpu_resource_lifecycle", ok,
                     ok ? "invalid handle/palette 257/face-range rejected; double release safe"
                        : fail);
    }

    // --------------------------------- 6. authentic data (when assets given)
    // Honest classification (B19): the SkinObject.rcc .chf population is of versions
    // the mobile parser deliberately does not implement (documented B11 gap: 243
    // "unsupported .chf version" entries), so a full authentic CHARACTER render is
    // normally a documented SKIP, not a GPU failure.  Authentic TEXTURES from the
    // original packs (.mtf/.dds) are decoded and rendered through the GPU path
    // whenever they are bundled — that part is original client bytes end-to-end.
    bool authenticAttempted = false;
    bool authenticPass = false;
    std::string authenticDetail = "no authentic assets provided";
    std::string screenshotNote = "no screenshot";
    {
        AuthCtx ctx{authenticAssetRoot, assetManager};

        // Scan the bundle manifest: one .chf + up to three .mtf/.dds textures.
        std::string chfName;
        std::vector<std::string> texNames;
        std::vector<uint8_t> manifest;
        if (AuthResolve(&ctx, "manifest.txt", manifest)) {
            std::string text(manifest.begin(), manifest.end());
            for (size_t i = 0; i < text.size();) {
                size_t j = i;
                while (j < text.size() && text[j] != '\n' && text[j] != '\r') ++j;
                const std::string line = text.substr(i, j - i);
                i = (j < text.size()) ? j + 1 : j;
                if (line.size() < 4) continue;
                const std::string ext = line.substr(line.size() - 4);
                auto lower = [](std::string s) {
                    for (auto& c : s) {
                        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
                    }
                    return s;
                };
                if (lower(ext) == ".chf" && chfName.empty()) chfName = line;
                if (lower(ext) == ".mtf" && texNames.size() < 3) texNames.push_back(line);
            }
        }

        // ---- 6a. authentic texture through the GPU pipeline ---------------------
        bool texAttempted = false;
        bool texPass = false;
        std::string texDetail = "no authentic textures in the bundle";
        for (const std::string& texName : texNames) {
            std::vector<uint8_t> texBytes;
            if (!AuthResolve(&ctx, texName, texBytes)) continue;
            texAttempted = true;
            std::vector<uint8_t> raw;
            int fileType = -1;
            if (texName.size() >= 4 &&
                texName.substr(texName.size() - 4) == ".mtf") {
                if (!EP7TextureDecoder::DecryptMTF(texBytes, raw, fileType)) {
                    texDetail = "MTF decrypt failed: " + texName;
                    continue;
                }
            } else {
                raw = texBytes;
            }
            DecodedTexture tex;
            std::string fmt;
            if (raw.size() >= 4 && std::memcmp(raw.data(), "DDS ", 4) == 0) {
                fmt = "dds";
                tex = EP7TextureDecoder::DecodeDDS(raw);
            } else {
                fmt = "tga";
                tex = EP7TextureDecoder::DecodeTGA(raw);
            }
            if (!tex.valid) {
                texDetail = "decode failed [" + fmt + "]: " + texName +
                            " (unsupported variant — reported, never fabricated)";
                continue;
            }
            // Render an NDC quad with the AUTHENTIC texture and sample it.
            const uint32_t texId = renderer.UploadRGBA8Texture(tex.width, tex.height, tex.rgba8.data());
            if (texId == 0) {
                texDetail = "upload failed for " + texName;
                continue;
            }
            const int W = 64;
            const int H = 64;
            renderer.Resize(W, H);
            glViewport(0, 0, W, H);
            const float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
            renderer.BeginScene3D(MobileCamera{});
            renderer.SetViewProjectionOverride(identity);
            EP7SkinnedMesh mesh = QuadMesh();
            const EP7GpuSkinStreams streams = BuildGpuSkinStreams(mesh);
            RANMobile::GpuSkinnedMeshData data;
            std::vector<float> interleaved(static_cast<size_t>(mesh.vertexCount) * 16, 0.0f);
            for (uint32_t v = 0; v < mesh.vertexCount; ++v) {
                float* d = interleaved.data() + static_cast<size_t>(v) * 16;
                for (int k = 0; k < 3; ++k) {
                    d[k] = mesh.positions[static_cast<size_t>(v) * 3 + k];
                    d[3 + k] = mesh.normals[static_cast<size_t>(v) * 3 + k];
                }
                d[6] = mesh.texcoords[static_cast<size_t>(v) * 2];
                d[7] = mesh.texcoords[static_cast<size_t>(v) * 2 + 1];
                for (int k = 0; k < 4; ++k) {
                    d[8 + k] = streams.boneIndices[static_cast<size_t>(v) * 4 + k];
                    d[12 + k] = streams.boneWeights[static_cast<size_t>(v) * 4 + k];
                }
            }
            data.interleaved = interleaved.data();
            data.vertexCount = mesh.vertexCount;
            data.indices = mesh.faces.data();
            data.indexCount = static_cast<uint32_t>(mesh.faces.size());
            const uint32_t handle = renderer.UploadGpuSkinnedMesh(data);
            std::vector<float> paletteRow(32, 0.0f);
            for (int m = 0; m < 2; ++m) {
                for (int r = 0; r < 4; ++r) {
                    paletteRow[static_cast<size_t>(m) * 16 + static_cast<size_t>(r) * 4 + r] = 1.0f;
                }
            }
            const float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
            glClearColor(0.05f, 0.05f, 0.10f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            renderer.DrawGpuSkinnedMesh(handle, paletteRow.data(), 2, texId, white, false, 0, 2);
            uint8_t px[4] = {0, 0, 0, 0};
            uint8_t bg[4] = {13, 13, 26, 255};
            glReadPixels(W / 2, H / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
            // "Something from the texture came through" = clearly not the clear colour
            // (an exact texel expectation is impossible for arbitrary authentic art).
            texPass = std::fabs(static_cast<float>(px[0]) - bg[0]) > 8.0f ||
                      std::fabs(static_cast<float>(px[1]) - bg[1]) > 8.0f ||
                      std::fabs(static_cast<float>(px[2]) - bg[2]) > 8.0f;
            char buf[256];
            std::snprintf(buf, sizeof(buf), "%s [%s] %ux%u sampled=(%u,%u,%u,%u) bg=(%u,%u,%u)",
                          texName.c_str(), fmt.c_str(), tex.width, tex.height, px[0], px[1], px[2],
                          px[3], bg[0], bg[1], bg[2]);
            texDetail = buf;
            renderer.ReleaseGpuSkinnedMesh(handle);
            renderer.DeleteTexture(texId);
            break;  // one successfully rendered authentic texture is the evidence
        }
        tests.Record("authentic_texture_gpu", texAttempted ? texPass : true,
                     texAttempted ? texDetail : "SKIPPED — " + texDetail +
                         " (informational: no texture bundled is not a GPU failure)");
        json += "    \"authenticTexture\": {\"attempted\": " +
                std::string(texAttempted ? "true" : "false") + ", \"pass\": " +
                std::string(texPass ? "true" : "false") + ", \"detail\": " + JStr(texDetail) + "},\n";

        // ---- 6b. authentic character (documented version gap = SKIP) ------------
        std::string pieceStats = "[]";
        if (chfName.empty()) {
            tests.Record("authentic_character_render", true,
                         "SKIPPED — no authentic .chf bundled (informational, not a GPU failure)");
        } else {
            authenticAttempted = true;
            std::vector<uint8_t> chfBytes;
            AuthResolve(&ctx, chfName, chfBytes);
            EP7CharacterLoader loader;
            loader.SetResolver(&AuthResolve, &ctx);
            EP7CharacterScene scene;
            scene.SetRenderer(&renderer);
            scene.SetTextureResolver(&AuthResolve, &ctx);
            EP7CharacterScene::AssetPtr asset =
                std::make_shared<EP7CharacterAsset>(loader.LoadCharacter(chfBytes, false));
            const bool versionGap = asset->error.find("unsupported .chf version") != std::string::npos;
            if (!asset->loaded && versionGap) {
                authenticPass = true;  // documented B11 gap — verdict-neutral
                authenticDetail = "SKIPPED — " + asset->error +
                                  " (mobile parser implements 0x0100..0x0102; SkinObject.rcc ships"
                                  " later versions — documented B11 gap, no fabricated substitute)";
                tests.Record("authentic_character_render", true, authenticDetail);
            } else if (!asset->loaded) {
                authenticPass = false;
                authenticDetail = "authentic .chf failed to load: " + asset->error;
                tests.Record("authentic_character_render", false, authenticDetail);
            } else {
                // (full render path — only reachable for loadable .chf versions)
                const bool registered = scene.RegisterAsset(asset);
                const uint32_t instanceId = registered ? scene.Spawn(asset, 0.0f, 0.0f, 0.0f, 0.0f) : 0;
                EP7CharacterInstance* instance = instanceId != 0 ? scene.Find(instanceId) : nullptr;
                if (!registered || !instance) {
                    authenticPass = false;
                    authenticDetail = "scene registration/spawn failed";
                    tests.Record("authentic_character_render", false, authenticDetail);
                } else {
                    EP7CharacterScene::EvaluatePose(*instance, nullptr);
                    const int W = 256;
                    const int H = 256;
                    renderer.Resize(W, H);
                    glViewport(0, 0, W, H);
                    MobileCamera camera;
                    camera.SetTarget(0.0f, 60.0f, 0.0f);
                    camera.SetDistanceBounds(40.0f, 400.0f);
                    renderer.BeginScene3D(camera);
                    glClearColor(0.05f, 0.05f, 0.10f, 1.0f);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                    uint32_t piecesDrawn = 0;
                    uint32_t piecesSkipped = 0;
                    uint32_t texturesResolved = 0;
                    uint32_t texturesMissing = 0;
                    pieceStats = "[";
                    for (uint32_t slot = 0; slot < RANMobile::kEP7PieceSize; ++slot) {
                        const EP7CharacterPiece& piece = asset->pieces[slot];
                        if (!piece.loaded || piece.mesh.faces.empty()) continue;
                        EP7CharacterScene::EP7SubsetValidation sv;
                        const bool svOk = scene.ValidatePieceSubsets(asset, slot, &sv);
                        const uint32_t handle = scene.EnsurePieceGpuHandle(asset, slot);
                        const float* palette = nullptr;
                        uint32_t paletteCount = 0;
                        const bool havePalette =
                            scene.GetPiecePalette(*instance, slot, &palette, &paletteCount);
                        if (pieceStats.size() > 1) pieceStats += ",";
                        pieceStats += "{\"slot\": " + std::to_string(slot) +
                                      ", \"subsets\": " + std::to_string(sv.subsetCount) +
                                      ", \"materials\": " + std::to_string(sv.materialCount) +
                                      ", \"subsetValidation\": " + (svOk ? "true" : "false") +
                                      ", \"gpuHandle\": " + std::to_string(handle != 0) +
                                      ", \"palette\": " + std::to_string(paletteCount) +
                                      ", \"verts\": " + std::to_string(piece.mesh.vertexCount) + "}";
                        if (handle == 0 || !havePalette || !svOk) {
                            ++piecesSkipped;
                            continue;
                        }
                        const auto& subsets = piece.mesh.subsets;
                        if (subsets.empty()) {
                            const float flat[4] = {0.75f, 0.75f, 0.75f, 1.0f};
                            if (renderer.DrawGpuSkinnedMesh(handle, palette, paletteCount, 0, flat,
                                                            false, 0,
                                                            static_cast<uint32_t>(
                                                                piece.mesh.faces.size() / 3))) {
                                ++piecesDrawn;
                            }
                        } else {
                            bool drew = false;
                            for (uint32_t s = 0; s < subsets.size(); ++s) {
                                if (subsets[s].faceCount == 0) continue;
                                const uint32_t texId = scene.GetPieceTextureId(asset, slot, s);
                                if (texId != 0) {
                                    ++texturesResolved;
                                } else {
                                    ++texturesMissing;
                                }
                                bool alphaBlend = false;
                                if (subsets[s].materialIndex < piece.info.materials.size()) {
                                    alphaBlend =
                                        (piece.info.materials[subsets[s].materialIndex].flags & 0x0001u) != 0;
                                }
                                const float texColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
                                const float flat[4] = {0.75f, 0.75f, 0.75f, 1.0f};
                                drew = renderer.DrawGpuSkinnedMesh(
                                           handle, palette, paletteCount, texId,
                                           texId != 0 ? texColor : flat, alphaBlend,
                                           subsets[s].faceStart, subsets[s].faceCount) ||
                                       drew;
                            }
                            if (drew) ++piecesDrawn;
                        }
                    }
                    pieceStats += "]";

                    std::vector<uint8_t> pixels(static_cast<size_t>(W) * H * 4);
                    glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                    uint32_t litPixels = 0;
                    for (size_t i = 0; i < pixels.size(); i += 4) {
                        if (pixels[i] > 40 || pixels[i + 1] > 40 || pixels[i + 2] > 40) ++litPixels;
                    }
                    if (!screenshotOutPath.empty()) {
                        std::FILE* ppm = std::fopen(screenshotOutPath.c_str(), "wb");
                        if (ppm) {
                            std::fprintf(ppm, "P6\n%d %d\n255\n", W, H);
                            for (int y = H - 1; y >= 0; --y) {
                                std::fwrite(&pixels[static_cast<size_t>(y) * W * 4], 1,
                                            static_cast<size_t>(W) * 4, ppm);
                            }
                            std::fclose(ppm);
                            result.screenshotPath = screenshotOutPath;
                            screenshotNote = screenshotOutPath;
                        }
                    }
                    scene.Clear();
                    authenticPass = piecesDrawn > 0;
                    char buf[224];
                    std::snprintf(buf, sizeof(buf),
                                  "piecesDrawn=%u skipped=%u texResolved=%u texMissing=%u litPixels=%u/%d",
                                  piecesDrawn, piecesSkipped, texturesResolved, texturesMissing,
                                  litPixels, W * H);
                    authenticDetail = buf;
                    tests.Record("authentic_character_render", authenticPass,
                                 std::string(buf) + " — original EP7 client files, " + chfName);
                }
            }
        }
        json += "    \"authentic\": {\"attempted\": " + std::string(authenticAttempted ? "true" : "false") +
                ", \"pass\": " + std::string(authenticPass ? "true" : "false") +
                ", \"chf\": " + JStr(chfName) +
                ", \"detail\": " + JStr(authenticDetail) +
                ", \"pieces\": " + pieceStats +
                ", \"screenshot\": " + JStr(screenshotNote) + "},\n";
    }

    // ------------------------------------------------------------------ verdict
    json += "    \"tests\": [\n        " + tests.entries + "\n    ],\n";
    result.overallPass = tests.pass;
    json += "    \"verdict\": \"" + std::string(result.overallPass ? "PASS" : "FAIL") + "\"\n}";
    result.json = json;
    return result;
#endif
}

} // namespace RANMobile
