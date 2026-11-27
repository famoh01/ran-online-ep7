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
    mesh.vertexCount = 4;
    mesh.vertexStride = 32;
    const float pos[4][3] = {{-1.0f, -1.0f, 0.0f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}};
    const float pos2[4][3] = {{0.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};
    (void)pos2;
    for (int v = 0; v < 4; ++v) {
        for (int k = 0; k < 3; ++k) mesh.positions.push_back(pos[v][k]);
        mesh.normals.push_back(0.0f);
        mesh.normals.push_back(0.0f);
        mesh.normals.push_back(1.0f);
        mesh.texcoords.push_back((pos[v][0] + 1.0f) * 0.5f);
        mesh.texcoords.push_back((pos[v][1] + 1.0f) * 0.5f);
    }
    mesh.faces = {0, 1, 2, 0, 2, 3};  // 2 triangles, one per subset
    EP7MeshSubset s0;
    s0.materialIndex = 0;
    s0.attributeId = 0;
    s0.faceStart = 0;
    s0.faceCount = 1;
    EP7MeshSubset s1;
    s1.materialIndex = 1;
    s1.attributeId = 1;
    s1.faceStart = 1;
    s1.faceCount = 1;
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
        tests.Record("shader_compile_link", ready,
                     ready ? "GPU skin program linked on live GL (renderer Initialize)"
                           : "GPU skin program failed to compile/link — see driver logs");
        tests.Record("uniform_block_capacity", maxBlock >= 16384,
                     "MAX_UNIFORM_BLOCK_SIZE=" + std::to_string(maxBlock) +
                         " required>=16384 (256 mat4 palette)");
        const char* vs = MobileRenderer::CharGpuVertexShaderSource();
        const char* fs = MobileRenderer::CharGpuFragmentShaderSource();
        tests.Record("shader_source_available", vs != nullptr && fs != nullptr,
                     "production GLSL sources inspected by harness");
    }

    if (!renderer.IsInitialized()) {
        renderer.Initialize(64, 64);
    }

    // ------------------------------- 3. CPU vs GPU skinning (transform feedback)
    auto tfCompare = [&](const char* label, const EP7SkinnedMesh& mesh,
                         const std::vector<ep7math::Mat4>& palette) -> bool {
        const EP7GpuSkinStreams streams = BuildGpuSkinStreams(mesh);
        if (!streams.valid) return false;
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
        if (handle == 0) return false;

        // CPU reference (SkinVerticesCPU — same palette semantics as B17).
        std::vector<float> cpuPos;
        std::vector<float> cpuNrm;
        if (!SkinVerticesCPU(mesh, palette, cpuPos, cpuNrm)) {
            renderer.ReleaseGpuSkinnedMesh(handle);
            return false;
        }

        // GPU capture via transform feedback on the PRODUCTION program (linked
        // with the vSkinnedPos/vNormal capture set by the renderer).
        const GLuint tfBuffer = [&] {
            GLuint b = 0;
            glGenBuffers(1, &b);
            glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, b);
            const GLsizeiptr bytes =
                static_cast<GLsizeiptr>(mesh.vertexCount) * 6 * static_cast<GLsizeiptr>(sizeof(float));
            glBufferData(GL_TRANSFORM_FEEDBACK_BUFFER, bytes, nullptr, GL_STREAM_READ);
            glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, 0);
            return b;
        }();
        GLuint tfObj = 0;
        glGenTransformFeedbacks(1, &tfObj);
        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, tfObj);
        glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, tfBuffer);

        // Identity VP: gl_Position == skinned position (w == 1, affine parity).
        const float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        renderer.BeginScene3D(MobileCamera{});  // must precede draws (sets 3D pass state)
        renderer.SetViewProjectionOverride(identity);

        // Upload palette (offset*combined, row-major) — the exact draw path.
        std::vector<float> paletteRow;
        paletteRow.reserve(palette.size() * 16);
        for (const auto& m : palette) {
            for (int r = 0; r < 4; ++r) {
                for (int c = 0; c < 4; ++c) paletteRow.push_back(m.m[r][c]);
            }
        }
        const float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        glEnable(GL_RASTERIZER_DISCARD);
        glBeginTransformFeedback(GL_TRIANGLES);
        const bool drew = renderer.DrawGpuSkinnedMesh(handle, paletteRow.data(),
                                                      static_cast<uint32_t>(palette.size()), 0, white,
                                                      false, 0,
                                                      static_cast<uint32_t>(mesh.faces.size() / 3));
        glEndTransformFeedback();
        glDisable(GL_RASTERIZER_DISCARD);

        bool match = drew;
        float maxPosDiff = 0.0f;
        float maxNrmDiff = 0.0f;
        if (drew) {
            glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, tfBuffer);
            const float* captured = static_cast<const float*>(
                glMapBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, 0,
                                 static_cast<GLsizeiptr>(mesh.vertexCount) * 6 * sizeof(float),
                                 GL_MAP_READ_BIT));
            if (captured) {
                for (uint32_t v = 0; v < mesh.vertexCount; ++v) {
                    for (int k = 0; k < 3; ++k) {
                        const float gpuPos = captured[static_cast<size_t>(v) * 6 + k];
                        const float cpu = cpuPos[static_cast<size_t>(v) * 3 + k];
                        maxPosDiff = std::fmax(maxPosDiff, std::fabs(gpuPos - cpu));
                        const float gpuNrm = captured[static_cast<size_t>(v) * 6 + 3 + k];
                        const float cpuN = cpuNrm[static_cast<size_t>(v) * 3 + k];
                        maxNrmDiff = std::fmax(maxNrmDiff, std::fabs(gpuNrm - cpuN));
                    }
                }
                // Positions: float32 vs float32 (same math, different unit) — 5e-4
                // absolute is generous against MAD rounding on O(1..16) values.
                // Normals include per-unit normalize rounding — 5e-3.
                match = maxPosDiff <= 5e-4f && maxNrmDiff <= 5e-3f;
                glUnmapBuffer(GL_TRANSFORM_FEEDBACK_BUFFER);
            } else {
                match = false;
            }
            glBindBuffer(GL_TRANSFORM_FEEDBACK_BUFFER, 0);
        }
        glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
        glDeleteTransformFeedbacks(1, &tfObj);
        glDeleteBuffers(1, &tfBuffer);
        renderer.ReleaseGpuSkinnedMesh(handle);

        char detail[160];
        std::snprintf(detail, sizeof(detail), "maxPosDiff=%.2e maxNrmDiff=%.2e (tol 5e-4 / 5e-3)",
                      maxPosDiff, maxNrmDiff);
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
            tfCompare("cpu_gpu_skinning_bindpose", mesh, bindPalette);

            // Animated pose: rotate the child bone 90 deg about Y (manual SQT),
            // then evaluate and compose the palette exactly like the runtime.
            EP7Bone& spine = const_cast<EP7Bone&>(skeleton.GetBone(1));
            spine.rotation = ep7math::Quat(0.0f, 0.70710678f, 0.0f, 0.70710678f);
            skeleton.UpdateBones(ep7math::Mat4(), 1.0f);
            std::vector<ep7math::Mat4> animPalette;
            BuildBonePalette(mesh, skeleton, animPalette);
            tfCompare("cpu_gpu_skinning_animated", mesh, animPalette);
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
            checkColor("raster_alpha_blend", px, 0.5f * lit, 0.0f, 0.5f * (1.0f - lit) + 0.5f * 1.0f,
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
            renderer.DrawGpuSkinnedMesh(ssHandle, paletteRow.data(), 2, 0, red, false, 0, 1);
            renderer.DrawGpuSkinnedMesh(ssHandle, paletteRow.data(), 2, 0, green, false, 1, 1);
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
        bool ok = handle != 0 && renderer.GpuMeshCount() == 1;
        // Invalid handle must fail loudly (never silently).
        ok = ok && !renderer.DrawGpuSkinnedMesh(handle + 999, paletteRow.data(), 2, 0, white, false, 0, 2);
        // Palette overflow must fail (never approximate).
        ok = ok && !renderer.DrawGpuSkinnedMesh(handle, paletteRow.data(), 257, 0, white, false, 0, 2);
        // Face-range OOB must fail.
        ok = ok && !renderer.DrawGpuSkinnedMesh(handle, paletteRow.data(), 2, 0, white, false, 2, 2);
        // Double release must be safe.
        renderer.ReleaseGpuSkinnedMesh(handle);
        const uint32_t before = renderer.GpuMeshCount();
        renderer.ReleaseGpuSkinnedMesh(handle);
        ok = ok && renderer.GpuMeshCount() == before - 1 && renderer.GpuMeshCount() == 0;
        tests.Record("gpu_resource_lifecycle", ok,
                     "invalid handle/palette 257/face-range rejected; double release safe");
    }

    // --------------------------------- 6. authentic character (when assets given)
    bool authenticAttempted = false;
    bool authenticPass = false;
    std::string authenticDetail = "no authentic assets provided";
    std::string screenshotNote = "no screenshot";
    {
        AuthCtx ctx{authenticAssetRoot, assetManager};

        // Find the .chf via the bundle manifest (CI writes manifest.txt next to the
        // bundled authentic files).
        std::string chfName;
        std::vector<uint8_t> manifest;
        if (AuthResolve(&ctx, "manifest.txt", manifest)) {
            std::string text(manifest.begin(), manifest.end());
            for (size_t i = 0; i < text.size();) {
                size_t j = i;
                while (j < text.size() && text[j] != '\n' && text[j] != '\r') ++j;
                const std::string line = text.substr(i, j - i);
                i = (j < text.size()) ? j + 1 : j;
                if (line.size() > 4 && line.substr(line.size() - 4) == ".chf") {
                    chfName = line;
                    break;
                }
            }
        }
        std::vector<uint8_t> chfBytes;
        if (!chfName.empty()) {
            AuthResolve(&ctx, chfName, chfBytes);
        }

        if (chfBytes.empty()) {
            tests.Record("authentic_character_render", true,
                         "SKIPPED — no authentic assets bundled (informational, not a GPU failure)");
        } else {
            authenticAttempted = true;
            std::string pieceStats = "[]";
            EP7CharacterLoader loader;
            loader.SetResolver(&AuthResolve, &ctx);
            EP7CharacterScene scene;
            scene.SetRenderer(&renderer);
            scene.SetTextureResolver(&AuthResolve, &ctx);
            EP7CharacterScene::AssetPtr asset =
                std::make_shared<EP7CharacterAsset>(loader.LoadCharacter(chfBytes, false));
            if (!asset->loaded) {
                authenticPass = false;
                authenticDetail = "authentic .chf failed to load: " + asset->error;
                tests.Record("authentic_character_render", false, authenticDetail);
            } else {
                const bool registered = scene.RegisterAsset(asset);
                const uint32_t instanceId = registered ? scene.Spawn(asset, 0.0f, 0.0f, 0.0f, 0.0f) : 0;
                EP7CharacterInstance* instance = instanceId != 0 ? scene.Find(instanceId) : nullptr;
                if (!registered || !instance) {
                    authenticPass = false;
                    authenticDetail = "scene registration/spawn failed";
                    tests.Record("authentic_character_render", false, authenticDetail);
                } else {
                    EP7CharacterScene::EvaluatePose(*instance, nullptr);
                    // Frame setup BEFORE drawing (viewport -> camera 3D pass -> clear).
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
                        // Draw the piece through the production path (palette + textures).
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

                    // Framebuffer capture: count non-background pixels + write PPM.
                    std::vector<uint8_t> pixels(static_cast<size_t>(W) * H * 4);
                    glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                    uint32_t litPixels = 0;
                    for (size_t i = 0; i < pixels.size(); i += 4) {
                        // Background is (13,13,26); anything clearly above that is geometry.
                        if (pixels[i] > 40 || pixels[i + 1] > 40 || pixels[i + 2] > 40) ++litPixels;
                    }
                    if (!screenshotOutPath.empty()) {
                        std::FILE* ppm = std::fopen(screenshotOutPath.c_str(), "wb");
                        if (ppm) {
                            std::fprintf(ppm, "P6\n%d %d\n255\n", W, H);
                            for (int y = H - 1; y >= 0; --y) {  // GL bottom-up -> PPM top-down
                                std::fwrite(&pixels[static_cast<size_t>(y) * W * 4], 1,
                                            static_cast<size_t>(W) * 4, ppm);
                            }
                            std::fclose(ppm);
                            result.screenshotPath = screenshotOutPath;
                            screenshotNote = screenshotOutPath;
                        }
                    }
                    scene.Clear();  // releases scene GPU resources (captures done)
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
            json += "    \"authentic\": {\"attempted\": " + std::string(authenticAttempted ? "true" : "false") +
                    ", \"pass\": " + std::string(authenticPass ? "true" : "false") +
                    ", \"chf\": " + JStr(chfName) +
                    ", \"detail\": " + JStr(authenticDetail) +
                    ", \"pieces\": " + pieceStats +
                    ", \"screenshot\": " + JStr(screenshotNote) + "},\n";
        }
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
