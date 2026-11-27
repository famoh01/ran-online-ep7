#include "EP7SkinnedMesh.h"

#include <cmath>
#include <cstring>

#include "EP7XFile.h"

namespace RANMobile {
namespace {

using ep7xfile::Token;

// Reads `count` Numbers from a data-object body, tolerating ',' and ';' separators.
bool ReadNumbers(const std::vector<Token>& tokens, size_t& pos, size_t count, float* out) {
    size_t read = 0;
    while (pos < tokens.size() && read < count) {
        const Token& t = tokens[pos];
        if (t.kind == Token::Kind::Number) {
            out[read++] = static_cast<float>(t.number);
            ++pos;
        } else if (t.kind == Token::Kind::Comma || t.kind == Token::Kind::Semicolon) {
            ++pos;
        } else if (t.kind == Token::Kind::RBrace) {
            return false;
        } else {
            return false;  // an identifier means the array ended early
        }
    }
    return read == count;
}

bool ReadDwords(const std::vector<Token>& tokens, size_t& pos, size_t count, uint32_t* out) {
    size_t read = 0;
    while (pos < tokens.size() && read < count) {
        const Token& t = tokens[pos];
        if (t.kind == Token::Kind::Number) {
            const double v = t.number;
            out[read++] = (v < 0.0) ? 0u : static_cast<uint32_t>(v);
            ++pos;
        } else if (t.kind == Token::Kind::Comma || t.kind == Token::Kind::Semicolon) {
            ++pos;
        } else if (t.kind == Token::Kind::RBrace) {
            return false;
        } else {
            return false;
        }
    }
    return read == count;
}

// Returns the index of the '}' matching the '{' at `openIndex`, or SIZE_MAX.
size_t MatchingBrace(const std::vector<Token>& tokens, size_t openIndex) {
    if (openIndex >= tokens.size() || tokens[openIndex].kind != Token::Kind::LBrace) return SIZE_MAX;
    int depth = 0;
    for (size_t i = openIndex; i < tokens.size(); ++i) {
        if (tokens[i].kind == Token::Kind::LBrace) ++depth;
        if (tokens[i].kind == Token::Kind::RBrace) {
            --depth;
            if (depth == 0) return i;
        }
    }
    return SIZE_MAX;
}

// SkinWeights { "bone"; n; indices; weights; matrix[16]; }
// `pos` starts at the token right after the `SkinWeights` identifier.
//
// D3DRM XTemplates declares `template SkinWeights { <guid> STRING transformNodeName;
// DWORD nWeights; ... }`, i.e. the object carries NO instance name: the bone name is the
// first element inside the body. Files written by D3DX may additionally carry an
// instance name before the brace, so both spellings are accepted.
bool ParseSkinWeights(const std::vector<Token>& tokens, size_t& pos, EP7SkinBoneInfo& bone,
                      size_t limit) {
    std::string instanceName;
    if (pos < limit &&
        (tokens[pos].kind == Token::Kind::String || tokens[pos].kind == Token::Kind::Identifier)) {
        instanceName = tokens[pos].text;
        ++pos;
    }
    if (pos >= limit || tokens[pos].kind != Token::Kind::LBrace) return false;
    const size_t close = MatchingBrace(tokens, pos);
    if (close == SIZE_MAX || close > limit) return false;
    ++pos;

    // The body must open with the transform-node (bone) name.
    std::string boneName = instanceName;
    while (pos < close) {
        const Token& t = tokens[pos];
        if (t.kind == Token::Kind::String || t.kind == Token::Kind::Identifier) {
            boneName = t.text;
            ++pos;
            break;
        }
        if (t.kind == Token::Kind::Number) {
            // No leading name at all: this is not a SkinWeights body.
            return false;
        }
        ++pos;
    }
    if (boneName.empty()) return false;

    uint32_t count = 0;
    bool haveCount = false;
    while (pos < close) {
        if (tokens[pos].kind == Token::Kind::Number) {
            count = static_cast<uint32_t>(tokens[pos].number);
            ++pos;
            haveCount = true;
            break;
        }
        if (tokens[pos].kind == Token::Kind::RBrace) return false;
        ++pos;
    }
    if (!haveCount || count > EP7SkinnedMeshReader::kMaxVertices) return false;

    bone.name = boneName;
    bone.vertices.resize(count);
    bone.weights.resize(count);

    if (count > 0) {
        if (!ReadDwords(tokens, pos, count, bone.vertices.data())) return false;
        if (!ReadNumbers(tokens, pos, count, bone.weights.data())) return false;
    }

    float matrix[16] = {0};
    if (!ReadNumbers(tokens, pos, 16, matrix)) return false;
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) bone.offsetMatrix.m[r][c] = matrix[r * 4 + c];
    }

    pos = close + 1;
    return true;
}

// MeshMaterialList { nMaterials; nFaceIndices; indices...; Material { ... } ... }
// We only need the diffuse texture names and the face -> material mapping, which is what
// D3DXLoadSkinMeshFromXof turns into D3DXMATERIAL[] for SMeshContainer.
void ParseMaterialList(const std::vector<Token>& tokens, size_t openBrace, size_t close,
                       std::vector<EP7MeshSubset>& subsets, std::vector<std::string>& textures) {
    size_t pos = openBrace + 1;

    // nMaterials
    uint32_t materialCount = 0;
    while (pos < close) {
        if (tokens[pos].kind == Token::Kind::Number) {
            materialCount = static_cast<uint32_t>(tokens[pos].number);
            ++pos;
            break;
        }
        if (tokens[pos].kind == Token::Kind::RBrace) return;
        ++pos;
    }

    // nFaceIndices + the index array
    uint32_t faceCount = 0;
    while (pos < close) {
        if (tokens[pos].kind == Token::Kind::Number) {
            faceCount = static_cast<uint32_t>(tokens[pos].number);
            ++pos;
            break;
        }
        if (tokens[pos].kind == Token::Kind::RBrace) return;
        ++pos;
    }
    if (faceCount > EP7SkinnedMeshReader::kMaxFaces) return;
    std::vector<uint32_t> faceMaterials(faceCount, 0);
    if (faceCount > 0 && !ReadDwords(tokens, pos, faceCount, faceMaterials.data())) return;

    // Material blocks in order.
    for (uint32_t m = 0; m < materialCount && pos < close; ++m) {
        while (pos < close && tokens[pos].kind != Token::Kind::Identifier) ++pos;
        if (pos >= close) break;
        // "Material" [name] '{'
        ++pos;
        if (pos < close && (tokens[pos].kind == Token::Kind::String || tokens[pos].kind == Token::Kind::Identifier)) {
            ++pos;
        }
        if (pos >= close || tokens[pos].kind != Token::Kind::LBrace) break;
        const size_t matClose = MatchingBrace(tokens, pos);
        if (matClose == SIZE_MAX || matClose > close) break;

        // Find the TextureFilename child: TextureFilename { "file.dds"; }
        std::string texture;
        for (size_t i = pos; i < matClose; ++i) {
            if (tokens[i].kind == Token::Kind::Identifier && tokens[i].text == "TextureFilename") {
                for (size_t j = i + 1; j < matClose; ++j) {
                    if (tokens[j].kind == Token::Kind::String) {
                        texture = tokens[j].text;
                        break;
                    }
                    if (tokens[j].kind == Token::Kind::RBrace) break;
                }
                break;
            }
        }
        if (texture.empty()) texture = "material_" + std::to_string(m);
        textures.push_back(texture);
        pos = matClose + 1;
    }

    // Collapse the face -> material array into contiguous subsets, exactly the way
    // SMeshContainer splits the index buffer (one draw per material).
    uint32_t start = 0;
    while (start < faceMaterials.size()) {
        const uint32_t material = faceMaterials[start];
        uint32_t end = start + 1;
        while (end < faceMaterials.size() && faceMaterials[end] == material) ++end;
        EP7MeshSubset subset;
        subset.materialIndex = material;
        subset.attributeId = material;
        subset.faceStart = start;
        subset.faceCount = end - start;
        subset.texture = material < textures.size() ? textures[material] : std::string();
        subsets.push_back(subset);
        start = end;
    }
}

}  // namespace

uint32_t EP7SkinnedMesh::MaxInfluencesPerVertex() const {
    // The HLSL declaration binds 4 weights (DxSkinMesh9_HLSL.cpp:104).
    return 4;
}

// ---------------------------------------------------------------------------
// EP7SkinnedMeshReader
// ---------------------------------------------------------------------------

EP7SkinnedMesh EP7SkinnedMeshReader::Parse(const std::vector<uint8_t>& data,
                                          const std::string& meshName,
                                          const EP7Skeleton* skeleton) {
    EP7SkinnedMesh mesh;

    std::vector<Token> tokens;
    std::string lexError;
    if (!ep7xfile::Tokenize(data.data(), data.size(), tokens, lexError)) {
        mesh.error = lexError;
        return mesh;
    }
    if (tokens.empty()) {
        mesh.error = "empty .x token stream";
        return mesh;
    }

    // ---- locate the wanted Mesh object ------------------------------------
    size_t meshKeyword = SIZE_MAX;
    size_t meshNamePos = SIZE_MAX;
    size_t meshOpen = SIZE_MAX;

    for (size_t i = 0; i < tokens.size(); ++i) {
        if (!(tokens[i].kind == Token::Kind::Identifier && tokens[i].text == "Mesh")) continue;

        size_t cursor = i + 1;
        std::string name;
        if (cursor < tokens.size() &&
            (tokens[cursor].kind == Token::Kind::String || tokens[cursor].kind == Token::Kind::Identifier)) {
            name = tokens[cursor].text;
            ++cursor;
        }
        if (cursor >= tokens.size() || tokens[cursor].kind != Token::Kind::LBrace) continue;

        if (meshName.empty() || name == meshName) {
            if (meshName.empty()) {
                meshKeyword = i;
                meshNamePos = (cursor > i + 1) ? i + 1 : SIZE_MAX;
                meshOpen = cursor;
                break;
            }
            // A named match wins immediately; keep scanning otherwise.
            meshKeyword = i;
            meshNamePos = i + 1;
            meshOpen = cursor;
            break;
        }
    }

    if (meshKeyword == SIZE_MAX) {
        mesh.error = meshName.empty() ? "no Mesh object found in .x" : ("mesh '" + meshName + "' not found in .x");
        return mesh;
    }
    const size_t meshClose = MatchingBrace(tokens, meshOpen);
    if (meshClose == SIZE_MAX) {
        mesh.error = "unterminated Mesh block";
        return mesh;
    }
    if (meshNamePos != SIZE_MAX && tokens[meshNamePos].kind != Token::Kind::LBrace) {
        mesh.name = tokens[meshNamePos].text;
    }

    // ---- faces ------------------------------------------------------------
    size_t pos = meshOpen + 1;
    uint32_t faceCount = 0;
    bool haveFaceCount = false;
    while (pos < meshClose) {
        if (tokens[pos].kind == Token::Kind::Number) {
            faceCount = static_cast<uint32_t>(tokens[pos].number);
            ++pos;
            haveFaceCount = true;
            break;
        }
        if (tokens[pos].kind != Token::Kind::Comma && tokens[pos].kind != Token::Kind::Semicolon) break;
        ++pos;
    }
    if (!haveFaceCount || faceCount == 0 || faceCount > kMaxFaces) {
        mesh.error = "invalid Mesh face count";
        return mesh;
    }
    mesh.faces.resize(static_cast<size_t>(faceCount) * 3);
    if (!ReadDwords(tokens, pos, static_cast<size_t>(faceCount) * 3, mesh.faces.data())) {
        mesh.error = "truncated Mesh face index array";
        return mesh;
    }

    // ---- vertex count + raw vertex stream ---------------------------------
    uint32_t vertexCount = 0;
    bool haveVertexCount = false;
    while (pos < meshClose) {
        if (tokens[pos].kind == Token::Kind::Number) {
            vertexCount = static_cast<uint32_t>(tokens[pos].number);
            ++pos;
            haveVertexCount = true;
            break;
        }
        if (tokens[pos].kind != Token::Kind::Comma && tokens[pos].kind != Token::Kind::Semicolon) break;
        ++pos;
    }
    if (!haveVertexCount || vertexCount == 0 || vertexCount > kMaxVertices) {
        mesh.error = "invalid Mesh vertex count";
        return mesh;
    }
    mesh.vertexCount = vertexCount;

    // D3DX writes the vertex buffer as `vertexCount * vertexSize` raw floats. Count the
    // contiguous run of numbers that follows to recover `vertexSize` (positions are
    // inferred by D3DX from that byte size plus the presence of MeshNormals /
    // MeshTextureCoords; we reproduce the same inference below).
    const size_t numbersStart = pos;
    size_t numberEnd = pos;
    while (numberEnd < meshClose) {
        const Token& u = tokens[numberEnd];
        if (u.kind == Token::Kind::Number || u.kind == Token::Kind::Comma || u.kind == Token::Kind::Semicolon) {
            ++numberEnd;
            continue;
        }
        break;
    }
    size_t totalNumbers = 0;
    for (size_t i = numbersStart; i < numberEnd; ++i) {
        if (tokens[i].kind == Token::Kind::Number) ++totalNumbers;
    }
    if (totalNumbers == 0 || (totalNumbers % vertexCount) != 0) {
        mesh.error = "vertex stream length is not a multiple of the vertex count";
        return mesh;
    }
    const size_t floatsPerVertex = totalNumbers / vertexCount;
    mesh.vertexStride = static_cast<uint32_t>(floatsPerVertex * sizeof(float));

    if (floatsPerVertex < 3) {
        mesh.error = "vertex stream is too small to hold a position";
        return mesh;
    }

    // Canonical EP7 layouts (see header comment): 3 / 6 / 8 floats.
    mesh.positions.resize(static_cast<size_t>(vertexCount) * 3);
    if (floatsPerVertex >= 6) mesh.normals.resize(static_cast<size_t>(vertexCount) * 3);
    if (floatsPerVertex == 8) mesh.texcoords.resize(static_cast<size_t>(vertexCount) * 2);

    for (uint32_t v = 0; v < vertexCount; ++v) {
        float f[64] = {0};
        const size_t take = floatsPerVertex > 64 ? 64 : floatsPerVertex;
        if (!ReadNumbers(tokens, pos, take, f)) {
            mesh.error = "truncated vertex buffer";
            return mesh;
        }
        for (size_t extra = take; extra < floatsPerVertex; ++extra) {
            if (pos < meshClose) ++pos;
        }
        mesh.positions[static_cast<size_t>(v) * 3 + 0] = f[0];
        mesh.positions[static_cast<size_t>(v) * 3 + 1] = f[1];
        mesh.positions[static_cast<size_t>(v) * 3 + 2] = f[2];
        if (floatsPerVertex >= 6) {
            mesh.normals[static_cast<size_t>(v) * 3 + 0] = f[3];
            mesh.normals[static_cast<size_t>(v) * 3 + 1] = f[4];
            mesh.normals[static_cast<size_t>(v) * 3 + 2] = f[5];
        }
        if (floatsPerVertex == 8) {
            mesh.texcoords[static_cast<size_t>(v) * 2 + 0] = f[6];
            mesh.texcoords[static_cast<size_t>(v) * 2 + 1] = f[7];
        }
    }

    mesh.geometryDecoded = true;
    if (floatsPerVertex == 3) {
        mesh.layout = EP7VertexLayout::Pos32;
    } else if (floatsPerVertex == 6) {
        mesh.layout = EP7VertexLayout::PosNormal32;
    } else if (floatsPerVertex == 8) {
        mesh.layout = EP7VertexLayout::PosNormalUV32;
    } else {
        mesh.layout = EP7VertexLayout::Unknown;
    }

    // ---- child objects: SkinWeights + MeshMaterialList --------------------
    // In the EP7 exporter (xskinexp/ExportXFile.cpp:2823) every SkinWeights object is
    // created and then AddDataObject()'d onto the Mesh, so skin data lives *inside* the
    // Mesh block. Some exporters emit them as following siblings; both are accepted.
    auto collectChildren = [&](size_t begin, size_t end) {
        size_t i = begin;
        while (i < end) {
            if (tokens[i].kind != Token::Kind::Identifier) {
                ++i;
                continue;
            }
            if (tokens[i].text == "SkinWeights") {
                size_t cursor = i + 1;
                EP7SkinBoneInfo bone;
                if (!ParseSkinWeights(tokens, cursor, bone, end)) {
                    mesh.error = "malformed SkinWeights object";
                    return false;
                }
                if (mesh.bones.size() >= kMaxBones) {
                    mesh.error = "too many skin bones";
                    return false;
                }
                mesh.bones.push_back(std::move(bone));
                i = cursor;
                continue;
            }
            if (tokens[i].text == "MeshMaterialList") {
                size_t brace = i + 1;
                while (brace < end && tokens[brace].kind != Token::Kind::LBrace) ++brace;
                const size_t close = MatchingBrace(tokens, brace);
                if (close != SIZE_MAX && close <= end) {
                    ParseMaterialList(tokens, brace, close, mesh.subsets, mesh.textures);
                    i = close + 1;
                    continue;
                }
            }
            ++i;
        }
        return true;
    };

    if (!collectChildren(pos, meshClose)) return mesh;

    if (mesh.bones.empty()) {
        // Fallback: sibling SkinWeights that immediately follow the Mesh.
        size_t i = meshClose + 1;
        while (i < tokens.size()) {
            if (tokens[i].kind == Token::Kind::Semicolon || tokens[i].kind == Token::Kind::Comma) {
                ++i;
                continue;
            }
            if (tokens[i].kind == Token::Kind::Identifier && tokens[i].text == "SkinWeights") {
                size_t cursor = i + 1;
                EP7SkinBoneInfo bone;
                if (!ParseSkinWeights(tokens, cursor, bone, tokens.size())) {
                    mesh.error = "malformed SkinWeights sibling object";
                    return mesh;
                }
                mesh.bones.push_back(std::move(bone));
                i = cursor;
                continue;
            }
            break;
        }
    }

    // ---- validation ------------------------------------------------------
    if (mesh.bones.empty()) {
        mesh.error = "mesh has no SkinWeights (not a skinned mesh)";
        return mesh;
    }

    if (skeleton && skeleton->IsValid()) {
        // DxSkinMesh9::SetupBoneMatrixPointersOnMesh fails when a bone is missing.
        for (auto& bone : mesh.bones) {
            bone.skeletonBoneIndex = skeleton->FindBoneIndex(bone.name);
            if (bone.skeletonBoneIndex < 0) {
                mesh.error = "SkinWeights reference a bone that is not in the skeleton: " + bone.name;
                return mesh;
            }
        }
    }

    for (const auto& bone : mesh.bones) {
        if (bone.vertices.size() != bone.weights.size()) {
            mesh.error = "SkinWeights index/weight count mismatch";
            return mesh;
        }
        for (uint32_t index : bone.vertices) {
            if (index >= mesh.vertexCount) {
                mesh.error = "SkinWeights vertex index out of range";
                return mesh;
            }
        }
        for (float weight : bone.weights) {
            if (!(weight >= 0.0f) || weight > 1.0f + 1e-4f) {
                mesh.error = "SkinWeights weight outside [0,1]";
                return mesh;
            }
        }
    }

    for (uint32_t index : mesh.faces) {
        if (index >= mesh.vertexCount) {
            mesh.error = "Mesh face index out of range";
            return mesh;
        }
    }

    mesh.valid = true;
    return mesh;
}

// ---------------------------------------------------------------------------
// Skinning
// ---------------------------------------------------------------------------

void BuildBonePalette(const EP7SkinnedMesh& mesh,
                      const EP7Skeleton& skeleton,
                      std::vector<ep7math::Mat4>& outPalette) {
    outPalette.clear();
    outPalette.reserve(mesh.bones.size());
    for (const auto& bone : mesh.bones) {
        ep7math::Mat4 combined = ep7math::Mat4::Identity();
        if (bone.skeletonBoneIndex >= 0 &&
            static_cast<size_t>(bone.skeletonBoneIndex) < skeleton.BoneCount()) {
            combined = skeleton.GetBone(static_cast<size_t>(bone.skeletonBoneIndex)).combined;
        }
        // DxSkinMesh9_CPU.cpp:625  g_pBoneMatrices[i] = pBoneOffsetMatrices[i] * ppBoneMatrixPtrs[i]
        outPalette.push_back(ep7math::Multiply(bone.offsetMatrix, combined));
    }
}

bool SkinVerticesCPU(const EP7SkinnedMesh& mesh,
                     const std::vector<ep7math::Mat4>& palette,
                     std::vector<float>& outPositions,
                     std::vector<float>& outNormals) {
    if (!mesh.valid || !mesh.geometryDecoded || mesh.vertexCount == 0) return false;
    if (mesh.positions.size() < static_cast<size_t>(mesh.vertexCount) * 3) return false;

    outPositions.assign(static_cast<size_t>(mesh.vertexCount) * 3, 0.0f);
    outNormals.assign(static_cast<size_t>(mesh.vertexCount) * 3, 0.0f);

    const bool haveNormals = mesh.normals.size() >= static_cast<size_t>(mesh.vertexCount) * 3;

    struct Influence {
        int32_t bone = -1;
        float weight = 0.0f;
    };
    std::vector<std::vector<Influence>> perVertex(mesh.vertexCount);

    for (size_t b = 0; b < mesh.bones.size() && b < palette.size(); ++b) {
        const auto& bone = mesh.bones[b];
        for (size_t k = 0; k < bone.vertices.size(); ++k) {
            const uint32_t v = bone.vertices[k];
            if (v >= mesh.vertexCount) continue;
            auto& list = perVertex[v];
            if (list.size() >= mesh.MaxInfluencesPerVertex()) continue;  // HLSL binds 4
            list.push_back({static_cast<int32_t>(b), bone.weights[k]});
        }
    }

    for (uint32_t v = 0; v < mesh.vertexCount; ++v) {
        const auto& list = perVertex[v];
        const ep7math::Vec3 p(mesh.positions[v * 3 + 0], mesh.positions[v * 3 + 1], mesh.positions[v * 3 + 2]);
        ep7math::Vec3 n(0.0f, 0.0f, 0.0f);
        if (haveNormals) n = ep7math::Vec3(mesh.normals[v * 3 + 0], mesh.normals[v * 3 + 1], mesh.normals[v * 3 + 2]);

        if (list.empty()) {
            outPositions[v * 3 + 0] = p.x;
            outPositions[v * 3 + 1] = p.y;
            outPositions[v * 3 + 2] = p.z;
            if (haveNormals) {
                outNormals[v * 3 + 0] = n.x;
                outNormals[v * 3 + 1] = n.y;
                outNormals[v * 3 + 2] = n.z;
            }
            continue;
        }

        float weightSum = 0.0f;
        ep7math::Vec3 acc(0.0f, 0.0f, 0.0f);
        ep7math::Vec3 accN(0.0f, 0.0f, 0.0f);
        for (const auto& infl : list) {
            if (infl.bone < 0 || static_cast<size_t>(infl.bone) >= palette.size()) continue;
            const ep7math::Mat4& m = palette[static_cast<size_t>(infl.bone)];
            const ep7math::Vec3 tp = ep7math::TransformCoord(p, m);
            acc.x += tp.x * infl.weight;
            acc.y += tp.y * infl.weight;
            acc.z += tp.z * infl.weight;
            if (haveNormals) {
                const ep7math::Vec3 tn = ep7math::TransformNormal(n, m);
                accN.x += tn.x * infl.weight;
                accN.y += tn.y * infl.weight;
                accN.z += tn.z * infl.weight;
            }
            weightSum += infl.weight;
        }

        if (weightSum > 1e-6f && std::fabs(weightSum - 1.0f) > 1e-4f) {
            acc.x /= weightSum;
            acc.y /= weightSum;
            acc.z /= weightSum;
            accN.x /= weightSum;
            accN.y /= weightSum;
            accN.z /= weightSum;
        }

        outPositions[v * 3 + 0] = acc.x;
        outPositions[v * 3 + 1] = acc.y;
        outPositions[v * 3 + 2] = acc.z;
        if (haveNormals) {
            const ep7math::Vec3 nn = ep7math::NormalizeVec3(accN);
            outNormals[v * 3 + 0] = nn.x;
            outNormals[v * 3 + 1] = nn.y;
            outNormals[v * 3 + 2] = nn.z;
        }
    }

    return true;
}

}  // namespace RANMobile
