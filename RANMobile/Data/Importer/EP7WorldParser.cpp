#include "EP7WorldParser.h"
#include "EP7HashVerify.h"
#include "EP7SerialFile.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <functional>
#include <sys/stat.h>
#include <unordered_map>
#include <unordered_set>

namespace RANMobile {

EP7Mat4 EP7Mat4::Identity() {
    return EP7Mat4{};
}

EP7Mat4 EP7Mat4::Multiply(const EP7Mat4& a, const EP7Mat4& b) {
    EP7Mat4 out;
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            float s = 0.0f;
            for (int k = 0; k < 4; ++k) {
                s += a.m[static_cast<size_t>(r) * 4u + static_cast<size_t>(k)] *
                     b.m[static_cast<size_t>(k) * 4u + static_cast<size_t>(c)];
            }
            out.m[static_cast<size_t>(r) * 4u + static_cast<size_t>(c)] = s;
        }
    }
    return out;
}

void EP7AABB::Expand(const EP7Vec3& p) {
    if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) ||
        std::fabs(p.x) > 1e7f || std::fabs(p.y) > 1e7f || std::fabs(p.z) > 1e7f) {
        return;
    }
    if (!valid) {
        min = p;
        max = p;
        valid = true;
        return;
    }
    min.x = std::min(min.x, p.x);
    min.y = std::min(min.y, p.y);
    min.z = std::min(min.z, p.z);
    max.x = std::max(max.x, p.x);
    max.y = std::max(max.y, p.y);
    max.z = std::max(max.z, p.z);
}

void EP7AABB::ExpandAABB(const EP7AABB& other) {
    if (!other.valid) return;
    Expand(other.min);
    Expand(other.max);
}

EP7Vec3 EP7AABB::Center() const {
    return {
        0.5f * (min.x + max.x),
        0.5f * (min.y + max.y),
        0.5f * (min.z + max.z)
    };
}

float EP7AABB::BoundingRadius() const {
    if (!valid) return 0.0f;
    const float dx = 0.5f * (max.x - min.x);
    const float dy = 0.5f * (max.y - min.y);
    const float dz = 0.5f * (max.z - min.z);
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

EP7AABB EP7AABB::Transform(const EP7Mat4& mat, const EP7AABB& local) {
    if (!local.valid) return {};
    EP7AABB out;
    const float xs[2] = { local.min.x, local.max.x };
    const float ys[2] = { local.min.y, local.max.y };
    const float zs[2] = { local.min.z, local.max.z };
    for (int ix = 0; ix < 2; ++ix) {
        for (int iy = 0; iy < 2; ++iy) {
            for (int iz = 0; iz < 2; ++iz) {
                const float x = xs[ix], y = ys[iy], z = zs[iz];
                // D3DX row-vector transform: v' = [x y z 1] * M
                const EP7Vec3 tp{
                    x * mat.m[0] + y * mat.m[4] + z * mat.m[8]  + mat.m[12],
                    x * mat.m[1] + y * mat.m[5] + z * mat.m[9]  + mat.m[13],
                    x * mat.m[2] + y * mat.m[6] + z * mat.m[10] + mat.m[14]
                };
                out.Expand(tp);
            }
        }
    }
    return out;
}

size_t EP7WorldData::TotalTerrainVertices() const {
    size_t total = 0;
    for (const auto& b : terrainBatches) total += b.vertices.size();
    return total;
}

size_t EP7WorldData::TotalTerrainTriangles() const {
    size_t total = 0;
    for (const auto& b : terrainBatches) total += b.indices.size() / 3u;
    return total;
}

namespace {

constexpr size_t kMaxMeshVerts = 2000000;
constexpr size_t kMaxMeshFaces = 2000000;
constexpr size_t kMaxListCount = 500000;

class Cursor {
public:
    Cursor(const uint8_t* data, size_t size, size_t startOffset = 0)
        : data_(data), size_(size), pos_(startOffset) {}

    size_t Pos() const { return pos_; }
    size_t Size() const { return size_; }
    size_t Remaining() const { return pos_ <= size_ ? (size_ - pos_) : 0; }
    bool Ok() const { return ok_; }
    const std::string& Error() const { return error_; }

    bool Fail(const std::string& msg) {
        if (ok_) {
            ok_ = false;
            error_ = msg + " at offset " + std::to_string(pos_);
        }
        return false;
    }

    bool Check(size_t n) {
        if (!ok_) return false;
        if (n > Remaining()) return Fail("Unexpected EOF needing " + std::to_string(n) + " bytes");
        return true;
    }

    bool Skip(size_t n) {
        if (!Check(n)) return false;
        pos_ += n;
        return true;
    }

    bool Seek(size_t absOffset) {
        if (!ok_) return false;
        if (absOffset > size_) return Fail("Seek out of bounds");
        pos_ = absOffset;
        return true;
    }

    template <typename T>
    bool ReadPOD(T& out) {
        if (!Check(sizeof(T))) return false;
        std::memcpy(&out, data_ + pos_, sizeof(T));
        pos_ += sizeof(T);
        return true;
    }

    bool ReadU8(uint8_t& v)   { return ReadPOD(v); }
    bool ReadU16(uint16_t& v) { return ReadPOD(v); }
    bool ReadU32(uint32_t& v) { return ReadPOD(v); }
    bool ReadI32(int32_t& v)  { return ReadPOD(v); }
    bool ReadF32(float& v)    { return ReadPOD(v); }
    bool ReadBOOL(bool& v)    { uint32_t raw = 0; if (!ReadU32(raw)) return false; v = (raw != 0); return true; }

    bool ReadVec3(EP7Vec3& v) {
        return ReadF32(v.x) && ReadF32(v.y) && ReadF32(v.z);
    }

    // EP7 files store D3DXVECTOR3 vMax then vMin in many structs.
    bool ReadMaxMinAABB(EP7AABB& out) {
        EP7Vec3 vmax, vmin;
        if (!ReadVec3(vmax) || !ReadVec3(vmin)) return false;
        out = EP7AABB{};
        out.Expand(vmin);
        out.Expand(vmax);
        return true;
    }

    bool ReadColor4(EP7Color4& c) {
        return ReadF32(c.r) && ReadF32(c.g) && ReadF32(c.b) && ReadF32(c.a);
    }

    bool ReadMat4(EP7Mat4& mat) {
        if (!Check(64)) return false;
        std::memcpy(mat.m.data(), data_ + pos_, 64);
        pos_ += 64;
        return true;
    }

    bool ReadFixedAscii(size_t len, std::string& out) {
        if (!Check(len)) return false;
        const char* s = reinterpret_cast<const char*>(data_ + pos_);
        size_t n = 0;
        while (n < len && s[n] != '\0') ++n;
        out.assign(s, n);
        pos_ += len;
        return true;
    }

    // CSerialFile >> std::string / CString: DWORD len (includes NUL if >0), followed by len chars.
    bool ReadSerialString(std::string& out) {
        uint32_t len = 0;
        if (!ReadU32(len)) return false;
        if (len == 0) {
            out.clear();
            return true;
        }
        if (len > 65536u || !Check(len)) return Fail("Invalid serial string length");
        const char* s = reinterpret_cast<const char*>(data_ + pos_);
        size_t n = 0;
        while (n < len && s[n] != '\0') ++n;
        out.assign(s, n);
        pos_ += len;
        return true;
    }

    // int len + raw char[len] without NUL terminator (DxFrame/DxMeshes/EffectLoadToList).
    bool ReadLenNoNulString(std::string& out) {
        int32_t len = 0;
        if (!ReadI32(len)) return false;
        if (len < 0 || static_cast<size_t>(len) > 65536u) return Fail("Invalid non-NUL string length");
        if (len == 0) {
            out.clear();
            return true;
        }
        if (!Check(static_cast<size_t>(len))) return false;
        const char* s = reinterpret_cast<const char*>(data_ + pos_);
        size_t n = 0;
        while (n < static_cast<size_t>(len) && s[n] != '\0') ++n;
        out.assign(s, n);
        pos_ += static_cast<size_t>(len);
        return true;
    }

    const uint8_t* Ptr() const { return data_ + pos_; }

private:
    const uint8_t* data_ = nullptr;
    size_t size_ = 0;
    size_t pos_ = 0;
    bool ok_ = true;
    std::string error_;
};

// Swizzles little-endian D3D9 D3DCOLOR (0xAARRGGBB -> bytes [B,G,R,A]) to GL RGBA ([R,G,B,A]).
inline uint32_t SwizzleD3DColorToRGBA(uint32_t d3dColor) {
    const uint32_t b = (d3dColor) & 0xFFu;
    const uint32_t g = (d3dColor >> 8) & 0xFFu;
    const uint32_t r = (d3dColor >> 16) & 0xFFu;
    const uint32_t a = (d3dColor >> 24) & 0xFFu;
    return r | (g << 8) | (b << 16) | (a << 24);
}

inline EP7Vec3 TransformPoint(const EP7Mat4& m, float x, float y, float z) {
    return {
        x * m.m[0] + y * m.m[4] + z * m.m[8]  + m.m[12],
        x * m.m[1] + y * m.m[5] + z * m.m[9]  + m.m[13],
        x * m.m[2] + y * m.m[6] + z * m.m[10] + m.m[14]
    };
}

inline EP7Vec3 TransformNormal(const EP7Mat4& m, float nx, float ny, float nz) {
    const float rx = nx * m.m[0] + ny * m.m[4] + nz * m.m[8];
    const float ry = nx * m.m[1] + ny * m.m[5] + nz * m.m[9];
    const float rz = nx * m.m[2] + ny * m.m[6] + nz * m.m[10];
    const float len = std::sqrt(rx * rx + ry * ry + rz * rz);
    if (len > 1e-6f) return { rx / len, ry / len, rz / len };
    return { 0.0f, 1.0f, 0.0f };
}

EP7AlphaBucket BucketFromExMaterialType(uint32_t d3dexType, const std::string& texName) {
    // D3DEXMATERIAL::MATERIAL_TYPE (enginelib/Meshs/DxMaterial.h):
    // 0=NORMAL, 1=ALPHA_HARD, 2=ALPHA_SOFT, 3=ALPHA_NOZWRI, 4=ALPHA_NOTEST, 5=ALPHA_SOFT_NOZWRI
    if (d3dexType == 1) return EP7AlphaBucket::HardAlpha;
    if (d3dexType >= 2) return EP7AlphaBucket::SoftAlpha;
    const std::string lower = EP7HashVerify::LowerAscii(texName);
    if (lower.find("_a1.") != std::string::npos) return EP7AlphaBucket::SoftAlpha01;
    if (lower.find("_a2.") != std::string::npos) return EP7AlphaBucket::SoftAlpha02;
    if (lower.find("_a.")  != std::string::npos) return EP7AlphaBucket::HardAlpha;
    return EP7AlphaBucket::Opaque;
}

// Skips a single DxFrame::LoadEffect / EffectLoadToList entry.
bool ReadFrameEffect(Cursor& c) {
    bool bName = false;
    if (!c.ReadBOOL(bName)) return false;
    if (bName) {
        std::string name;
        if (!c.ReadLenNoNulString(name)) return false;
    }

    uint32_t typeId = 0, dwVer = 0, dwSize = 0;
    if (!c.ReadU32(typeId) || !c.ReadU32(dwVer) || !c.ReadU32(dwSize)) return false;
    if (dwSize > 1048576u || !c.Skip(dwSize)) return false;

    bool bAffine = false;
    if (!c.ReadBOOL(bAffine)) return false;
    if (bAffine && !c.Skip(36)) return false;

    if (typeId == 0x2015u) return true; // DEF_EFFECT_TILE
    if (typeId == 0x2014u && (dwVer == 0x0101u || dwVer == 0x0102u)) {
        uint32_t s1 = 0, s2 = 0;
        if (!c.ReadU32(s1) || !c.Skip(s1)) return false;
        if (!c.ReadU32(s2) || !c.Skip(s2)) return false;
        return true;
    }
    uint32_t dwBuffSize = 0;
    if (!c.ReadU32(dwBuffSize)) return false;
    return c.Skip(dwBuffSize);
}

// Reads DWORD dwMaxCount followed by dwMaxCount EffectLoadToList entries.
bool ReadCountedEffectList(Cursor& c) {
    uint32_t dwMaxCount = 0;
    if (!c.ReadU32(dwMaxCount)) return false;
    if (dwMaxCount > 16384u) return c.Fail("Effect list count overflow");
    for (uint32_t i = 0; i < dwMaxCount; ++i) {
        if (!ReadFrameEffect(c)) return false;
    }
    return true;
}

// Reads DxOctreeMesh::SaveFile -> loads raw 32-byte OCTREEVERTEX mesh + attribute ranges.
bool ReadOctreeMeshRaw(Cursor& c,
                       std::vector<EP7WorldVertex>& outVerts,
                       std::vector<uint16_t>& outIndices,
                       std::vector<std::tuple<uint32_t, uint32_t, uint32_t>>& outAttrRanges) {
    uint32_t dwFVF = 0;
    uint32_t dwNumVertices = 0;
    if (!c.ReadU32(dwFVF) || !c.ReadU32(dwNumVertices)) return false;
    if (dwNumVertices > kMaxMeshVerts) return c.Fail("OctreeMesh vertex count overflow");

    size_t vStride = 32;
    if (dwFVF == 0x142u) vStride = 24;
    else if (dwFVF == 0x152u) vStride = 36;
    else if (dwFVF == 0x112u) vStride = 32;

    if (!c.Check(static_cast<size_t>(dwNumVertices) * vStride)) return false;
    outVerts.resize(dwNumVertices);
    for (uint32_t i = 0; i < dwNumVertices; ++i) {
        EP7WorldVertex v;
        if (!c.ReadF32(v.x) || !c.ReadF32(v.y) || !c.ReadF32(v.z)) return false;
        if (dwFVF == 0x112u) {
            if (!c.ReadF32(v.nx) || !c.ReadF32(v.ny) || !c.ReadF32(v.nz) ||
                !c.ReadF32(v.u) || !c.ReadF32(v.v)) return false;
            v.rgba = 0xFFFFFFFFu;
        } else if (dwFVF == 0x142u) {
            uint32_t d3dCol = 0xFFFFFFFFu;
            if (!c.ReadU32(d3dCol) || !c.ReadF32(v.u) || !c.ReadF32(v.v)) return false;
            v.rgba = SwizzleD3DColorToRGBA(d3dCol);
        } else if (dwFVF == 0x152u) {
            uint32_t d3dCol = 0xFFFFFFFFu;
            if (!c.ReadF32(v.nx) || !c.ReadF32(v.ny) || !c.ReadF32(v.nz) ||
                !c.ReadU32(d3dCol) || !c.ReadF32(v.u) || !c.ReadF32(v.v)) return false;
            v.rgba = SwizzleD3DColorToRGBA(d3dCol);
        } else {
            if (!c.Skip(vStride - 12u)) return false;
        }
        outVerts[i] = v;
    }

    uint32_t dwNumFaces = 0;
    if (!c.ReadU32(dwNumFaces)) return false;
    if (dwNumFaces > kMaxMeshFaces) return c.Fail("OctreeMesh face count overflow");
    if (!c.Check(static_cast<size_t>(dwNumFaces) * 6u)) return false;
    outIndices.resize(static_cast<size_t>(dwNumFaces) * 3u);
    for (size_t i = 0; i < outIndices.size(); ++i) {
        if (!c.ReadU16(outIndices[i])) return false;
    }

    uint32_t dwAttribSize = 0;
    if (!c.ReadU32(dwAttribSize)) return false;
    if (dwAttribSize > 65536u) return c.Fail("Attrib table overflow");
    outAttrRanges.clear();
    outAttrRanges.reserve(dwAttribSize);
    for (uint32_t i = 0; i < dwAttribSize; ++i) {
        uint32_t attribId = 0, faceStart = 0, faceCount = 0, vStart = 0, vCount = 0;
        if (!c.ReadU32(attribId) || !c.ReadU32(faceStart) || !c.ReadU32(faceCount) ||
            !c.ReadU32(vStart) || !c.ReadU32(vCount)) return false;
        outAttrRanges.emplace_back(attribId, faceStart, faceCount);
    }

    bool bAABB = false;
    if (!c.ReadBOOL(bAABB)) return false;
    if (bAABB) {
        // Recursive DxAABBNode: vMax(12)+vMin(12)+dwFace(4)+bLeft(4)+bRight(4)
        std::function<bool()> skipAABB = [&]() -> bool {
            if (!c.Skip(28)) return false;
            bool bLeft = false, bRight = false;
            if (!c.ReadBOOL(bLeft)) return false;
            if (bLeft && !skipAABB()) return false;
            if (!c.ReadBOOL(bRight)) return false;
            if (bRight && !skipAABB()) return false;
            return true;
        };
        if (!skipAABB()) return false;
    }
    return true;
}

// Reads DxMeshes::LoadFile directly (caller checked BOOL bMeshes).
bool ReadDxMeshes(Cursor& c,
                  const EP7Mat4* transformOrNull,
                  std::vector<EP7MeshBatch>& outBatches,
                  EP7AABB& outBounds) {
    uint32_t cMaterials = 0;
    if (!c.ReadU32(cMaterials)) return false;
    if (cMaterials > 4096u) return c.Fail("Material count overflow");

    struct MatEntry {
        EP7Color4 diffuse{1.0f, 1.0f, 1.0f, 1.0f};
        EP7Color4 ambient{0.25f, 0.25f, 0.25f, 1.0f};
        uint32_t exType = 0;
        std::string texName;
    };
    std::vector<MatEntry> mats(cMaterials);

    if (cMaterials > 0) {
        for (uint32_t i = 0; i < cMaterials; ++i) {
            if (!c.ReadColor4(mats[i].diffuse) || !c.ReadColor4(mats[i].ambient) || !c.Skip(36)) {
                return false;
            }
        }
        for (uint32_t i = 0; i < cMaterials; ++i) {
            uint32_t bSubTex = 0;
            if (!c.ReadU32(mats[i].exType) || !c.ReadU32(bSubTex)) return false;
        }
        bool bTexExist = false;
        if (!c.ReadBOOL(bTexExist)) return false;
        if (bTexExist) {
            for (uint32_t i = 0; i < cMaterials; ++i) {
                if (!c.ReadLenNoNulString(mats[i].texName)) return false;
            }
        }
    }

    bool bNameExist = false;
    if (!c.ReadBOOL(bNameExist)) return false;
    if (bNameExist) {
        std::string szName;
        if (!c.ReadLenNoNulString(szName)) return false;
    }

    {
        std::vector<EP7WorldVertex> rawVerts;
        std::vector<uint16_t> rawIndices;
        std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> attrRanges;
        if (!ReadOctreeMeshRaw(c, rawVerts, rawIndices, attrRanges)) return false;

        if (transformOrNull) {
            for (auto& v : rawVerts) {
                const EP7Vec3 p = TransformPoint(*transformOrNull, v.x, v.y, v.z);
                const EP7Vec3 n = TransformNormal(*transformOrNull, v.nx, v.ny, v.nz);
                v.x = p.x; v.y = p.y; v.z = p.z;
                v.nx = n.x; v.ny = n.y; v.nz = n.z;
            }
        }

        if (attrRanges.empty() && !rawIndices.empty()) {
            attrRanges.emplace_back(0u, 0u, static_cast<uint32_t>(rawIndices.size() / 3u));
        }

        for (const auto& [attribId, faceStart, faceCount] : attrRanges) {
            if (faceCount == 0) continue;
            const size_t idxBegin = static_cast<size_t>(faceStart) * 3u;
            const size_t idxEnd = idxBegin + static_cast<size_t>(faceCount) * 3u;
            if (idxEnd > rawIndices.size()) continue;

            EP7MeshBatch batch;
            if (attribId < mats.size()) {
                batch.textureName = mats[attribId].texName;
                batch.materialDiffuse = mats[attribId].diffuse;
                batch.materialAmbient = mats[attribId].ambient;
                batch.bucket = BucketFromExMaterialType(mats[attribId].exType, mats[attribId].texName);
            } else if (!mats.empty()) {
                batch.textureName = mats[0].texName;
                batch.bucket = BucketFromExMaterialType(mats[0].exType, mats[0].texName);
            }

            if (attrRanges.size() == 1 && idxBegin == 0 && idxEnd == rawIndices.size()) {
                batch.vertices = rawVerts;
                batch.indices = rawIndices;
                for (const auto& v : batch.vertices) batch.bounds.Expand({v.x, v.y, v.z});
            } else {
                std::unordered_map<uint16_t, uint16_t> remap;
                remap.reserve(rawIndices.size());
                batch.indices.reserve(idxEnd - idxBegin);
                for (size_t i = idxBegin; i < idxEnd; ++i) {
                    const uint16_t oldIdx = rawIndices[i];
                    if (oldIdx >= rawVerts.size()) continue;
                    auto it = remap.find(oldIdx);
                    if (it == remap.end()) {
                        const uint16_t newIdx = static_cast<uint16_t>(batch.vertices.size());
                        remap[oldIdx] = newIdx;
                        const EP7WorldVertex& rv = rawVerts[oldIdx];
                        batch.vertices.push_back(rv);
                        batch.bounds.Expand({rv.x, rv.y, rv.z});
                        batch.indices.push_back(newIdx);
                    } else {
                        batch.indices.push_back(it->second);
                    }
                }
            }
            if (!batch.vertices.empty() && !batch.indices.empty()) {
                outBounds.ExpandAABB(batch.bounds);
                outBatches.push_back(std::move(batch));
            }
        }
    }

    bool bNext = false;
    if (!c.ReadBOOL(bNext)) return false;
    if (bNext) {
        if (!ReadDxMeshes(c, transformOrNull, outBatches, outBounds)) return false;
    }
    return true;
}

bool ReadDxFrameNode(Cursor& c,
                     bool bakeTransformIntoVertices,
                     std::vector<EP7MeshBatch>& outBatches,
                     EP7AABB& outBounds) {
    EP7Mat4 matRot = EP7Mat4::Identity();
    EP7Mat4 matRotOrig = EP7Mat4::Identity();
    EP7Mat4 matCombined = EP7Mat4::Identity();
    EP7AABB treeBounds;
    if (!c.ReadMaxMinAABB(treeBounds)) return false;
    if (!c.ReadMat4(matRot) || !c.ReadMat4(matRotOrig) || !c.ReadMat4(matCombined)) return false;

    bool bName = false;
    if (!c.ReadBOOL(bName)) return false;
    if (bName) {
        std::string szName;
        if (!c.ReadLenNoNulString(szName)) return false;
    }

    bool bMeshes = false;
    if (!c.ReadBOOL(bMeshes)) return false;
    if (bMeshes) {
        const EP7Mat4* xf = bakeTransformIntoVertices ? &matCombined : nullptr;
        if (!ReadDxMeshes(c, xf, outBatches, outBounds)) return false;
    }

    // DxFrame::LoadFile reads two DWORD effect counts (each entry via LoadEffect).
    for (int pass = 0; pass < 2; ++pass) {
        uint32_t effCount = 0;
        if (!c.ReadU32(effCount)) return false;
        if (effCount > 4096u) return c.Fail("DxFrame effect count overflow");
        for (uint32_t i = 0; i < effCount; ++i) {
            if (!ReadFrameEffect(c)) return false;
        }
    }

    bool bSibling = false, bChild = false;
    if (!c.ReadBOOL(bSibling)) return false;
    if (bSibling && !ReadDxFrameNode(c, bakeTransformIntoVertices, outBatches, outBounds)) return false;
    if (!c.ReadBOOL(bChild)) return false;
    if (bChild && !ReadDxFrameNode(c, bakeTransformIntoVertices, outBatches, outBounds)) return false;
    return true;
}

bool ReadDxFrameTree(Cursor& c,
                     bool bakeTransformIntoVertices,
                     std::vector<EP7MeshBatch>& outBatches,
                     EP7AABB& outBounds) {
    bool bExist = false;
    if (!c.ReadBOOL(bExist)) return false;
    if (!bExist) return true;
    return ReadDxFrameNode(c, bakeTransformIntoVertices, outBatches, outBounds);
}

// Reads DxOctree::LoadFile recursively (login.wld terrain / alpha meshes).
bool ReadDxOctree(Cursor& c,
                  std::vector<EP7MeshBatch>& outBatches,
                  std::vector<EP7OctreeNode>& outNodes,
                  EP7AABB& outWorldBounds) {
    bool bSubDivided = false;
    EP7AABB box;
    uint32_t dataAddr = 0, dataSize = 0;
    if (!c.ReadBOOL(bSubDivided) || !c.ReadMaxMinAABB(box) ||
        !c.ReadU32(dataAddr) || !c.ReadU32(dataSize)) {
        return false;
    }
    const size_t nodeIdx = outNodes.size();
    outNodes.emplace_back();
    outNodes[nodeIdx].bounds = box;

    bool bFrame = false;
    if (!c.ReadBOOL(bFrame)) return false;
    if (bFrame) {
        const size_t batchStart = outBatches.size();
        EP7AABB frameBounds;
        if (!ReadDxFrameNode(c, false, outBatches, frameBounds)) return false;
        outWorldBounds.ExpandAABB(frameBounds);
        if (!outNodes[nodeIdx].bounds.valid) outNodes[nodeIdx].bounds = frameBounds;
        for (size_t i = batchStart; i < outBatches.size(); ++i) {
            outNodes[nodeIdx].batchIndices.push_back(static_cast<uint32_t>(i));
        }
    }

    for (int child = 0; child < 8; ++child) {
        bool bChild = false;
        if (!c.ReadBOOL(bChild)) return false;
        if (bChild) {
            if (!ReadDxOctree(c, outBatches, outNodes, outWorldBounds)) return false;
        }
    }
    return true;
}

bool ReadDxAnimationChain(Cursor& c) {
    uint32_t cPos = 0, cRot = 0, cScale = 0, cMat = 0;
    if (!c.ReadU32(cPos) || !c.Skip(static_cast<size_t>(cPos) * 16u)) return false; // SPositionKey = 16
    if (!c.ReadU32(cRot) || !c.Skip(static_cast<size_t>(cRot) * 20u)) return false; // SRotateKey = 20
    if (!c.ReadU32(cScale) || !c.Skip(static_cast<size_t>(cScale) * 16u)) return false; // SScaleKey = 16
    if (!c.ReadU32(cMat) || !c.Skip(static_cast<size_t>(cMat) * 80u)) return false; // SMatrixKey = 80 (padded D3DXMATRIXA16)

    bool bNext = false;
    if (!c.ReadBOOL(bNext)) return false;
    if (bNext && !ReadDxAnimationChain(c)) return false;

    bool bName = false;
    if (!c.ReadBOOL(bName)) return false;
    if (bName) {
        std::string animName;
        if (!c.ReadLenNoNulString(animName)) return false;
    }
    return true;
}

bool ReadDxAnimationMan(Cursor& c,
                        std::vector<EP7MeshBatch>& outBatches,
                        EP7AABB& outBounds) {
    uint32_t aniType = 0;
    float curTime = 0.0f;
    if (!c.ReadU32(aniType) || !c.ReadF32(curTime)) return false;

    bool bFrameRoot = false;
    if (!c.ReadBOOL(bFrameRoot)) return false;
    if (bFrameRoot) {
        // Animated frame hierarchy (sky domes / clouds / props in login.wld)
        if (!ReadDxFrameNode(c, true, outBatches, outBounds)) return false;
    }

    bool bAnimHead = false;
    if (!c.ReadBOOL(bAnimHead)) return false;
    if (bAnimHead && !ReadDxAnimationChain(c)) return false;

    bool bNext = false;
    if (!c.ReadBOOL(bNext)) return false;
    if (bNext && !ReadDxAnimationMan(c, outBatches, outBounds)) return false;
    return true;
}

// Reads DxAABBOctree::LoadFile (used in .wld0 and .pie).
bool ReadDxAABBOctree(Cursor& c,
                      const std::string& textureName,
                      EP7AlphaBucket bucket,
                      std::vector<EP7MeshBatch>& outBatches,
                      EP7AABB& outBounds) {
    uint32_t dwVer = 0, dwSize = 0;
    if (!c.ReadU32(dwVer) || !c.ReadU32(dwSize)) return false;
    const size_t endPos = c.Pos() + dwSize;
    if (endPos > c.Size()) return c.Fail("DxAABBOctree block exceeds file size");

    if (dwVer == 0x0100u || dwVer == 0x0101u) {
        EP7AABB bounds;
        bool bRootExist = false;
        if (!c.ReadMaxMinAABB(bounds) || !c.ReadBOOL(bRootExist)) return false;
        outBounds.ExpandAABB(bounds);

        if (bRootExist) {
            std::function<bool()> readOctreeNode = [&]() -> bool {
                bool bNodeExist = false;
                if (!c.ReadBOOL(bNodeExist)) return false;
                if (!bNodeExist) return true;

                EP7AABB nodeBox;
                uint32_t bufSize = 0;
                bool bUse = false;
                if (!c.ReadMaxMinAABB(nodeBox) || !c.ReadU32(bufSize)) {
                    return false;
                }
                const size_t objEnd = c.Pos() + bufSize;
                if (objEnd > endPos) return c.Fail("OBJOCTree block overflows DxAABBOctree");
                if (!c.ReadBOOL(bUse)) return false;
                if (bUse) {
                    uint32_t meshVer = 0, fvf = 0, nv = 0, nf = 0, landType = 0;
                    if (!c.ReadU32(meshVer) || !c.ReadU32(fvf) || !c.ReadU32(nv) ||
                        !c.ReadU32(nf) || !c.ReadU32(landType)) {
                        return false;
                    }
                    (void)meshVer; (void)landType;
                    if (nv > kMaxMeshVerts || nf > kMaxMeshFaces) return c.Fail("DxAABBOctree mesh overflow");
                    EP7MeshBatch batch;
                    batch.textureName = textureName;
                    batch.bucket = bucket;
                    batch.bounds = nodeBox;
                    batch.vertices.resize(nv);
                    for (uint32_t vi = 0; vi < nv; ++vi) {
                        EP7WorldVertex v;
                        if (!c.ReadF32(v.x) || !c.ReadF32(v.y) || !c.ReadF32(v.z)) return false;
                        if (fvf == 0x112u) {
                            if (!c.ReadF32(v.nx) || !c.ReadF32(v.ny) || !c.ReadF32(v.nz) ||
                                !c.ReadF32(v.u) || !c.ReadF32(v.v)) return false;
                            v.rgba = 0xFFFFFFFFu;
                        } else if (fvf == 0x142u) {
                            uint32_t d3dCol = 0xFFFFFFFFu;
                            if (!c.ReadU32(d3dCol) || !c.ReadF32(v.u) || !c.ReadF32(v.v)) return false;
                            v.rgba = SwizzleD3DColorToRGBA(d3dCol);
                        } else if (fvf == 0x152u) {
                            uint32_t d3dCol = 0xFFFFFFFFu;
                            if (!c.ReadF32(v.nx) || !c.ReadF32(v.ny) || !c.ReadF32(v.nz) ||
                                !c.ReadU32(d3dCol) || !c.ReadF32(v.u) || !c.ReadF32(v.v)) return false;
                            v.rgba = SwizzleD3DColorToRGBA(d3dCol);
                        } else {
                            return c.Fail("Unsupported DxAABBOctree FVF");
                        }
                        batch.vertices[vi] = v;
                    }
                    batch.indices.resize(static_cast<size_t>(nf) * 3u);
                    for (size_t ii = 0; ii < batch.indices.size(); ++ii) {
                        if (!c.ReadU16(batch.indices[ii])) return false;
                    }
                    if (!batch.vertices.empty() && !batch.indices.empty()) {
                        outBatches.push_back(std::move(batch));
                    }
                }
                // Seek past optional DIRECTPOINTCOLOR & pCollsionTREE inside OBJOCTree::Save EMBLOCK_02
                if (!c.Seek(objEnd)) return false;
                if (!readOctreeNode()) return false; // left child
                if (!readOctreeNode()) return false; // right child
                return true;
            };
            if (!readOctreeNode()) return false;
        }
    }

    return c.Seek(endPos);
}

// Reads DxSingleTexMesh::Load (used in DxStaticMesh::Load and DxStaticAniFrame::Load).
bool ReadDxSingleTexMesh(Cursor& c,
                         EP7AlphaBucket bucket,
                         std::vector<EP7MeshBatch>& outBatches,
                         EP7AABB& outBounds) {
    uint32_t dwVer = 0, dwSize = 0;
    if (!c.ReadU32(dwVer) || !c.ReadU32(dwSize)) return false;
    const size_t headerEnd = c.Pos() + dwSize;
    if (headerEnd > c.Size()) return c.Fail("DxSingleTexMesh block exceeds file size");

    std::string texName;
    if (dwVer == 0x0100u) {
        if (!c.ReadSerialString(texName)) return false;
    } else if (dwVer == 0x0101u) {
        uint32_t dwFlag = 0;
        if (!c.ReadU32(dwFlag) || !c.ReadSerialString(texName)) return false;
        (void)dwFlag;
    } else if (dwVer == 0x0102u || dwVer == 0x0103u || dwVer == 0x0104u) {
        if (!c.ReadSerialString(texName)) return false;
    }
    if (!c.Seek(headerEnd)) return false;

    if (dwVer != 0x0104u) {
        bool bExist = false;
        if (!c.ReadBOOL(bExist)) return false;
        if (bExist) {
            if (!ReadDxAABBOctree(c, texName, bucket, outBatches, outBounds)) return false;
        }
    }
    return true;
}

bool ReadDxStaticMeshBucketList(Cursor& c,
                                uint32_t staticMeshVer,
                                EP7AlphaBucket bucket,
                                std::vector<EP7MeshBatch>& outBatches,
                                EP7AABB& outBounds) {
    uint32_t count = 0;
    if (!c.ReadU32(count)) return false;
    if (count > 100000u) return c.Fail("DxStaticMesh bucket count overflow");
    for (uint32_t i = 0; i < count; ++i) {
        std::string mapKey;
        if (!c.ReadSerialString(mapKey)) return false;
        if (staticMeshVer == 0x0104u) {
            uint32_t flag = 0;
            if (!c.ReadU32(flag)) return false;
            (void)flag;
        }
        if (!ReadDxSingleTexMesh(c, bucket, outBatches, outBounds)) return false;
    }
    return true;
}

bool ReadDxStaticMesh(Cursor& c,
                      std::vector<EP7MeshBatch>& outBatches,
                      EP7AABB& outBounds) {
    uint32_t dwVer = 0, dwSize = 0;
    if (!c.ReadU32(dwVer) || !c.ReadU32(dwSize)) return false;
    const size_t headerEnd = c.Pos() + dwSize;
    if (headerEnd > c.Size()) return c.Fail("DxStaticMesh header block exceeds file size");

    if (dwVer >= 0x0100u && dwVer <= 0x0200u) {
        if (dwSize >= 24u) {
            if (!c.ReadMaxMinAABB(outBounds)) return false;
        }
        if (!c.Seek(headerEnd)) return false;

        if (dwVer <= 0x0101u) {
            if (!ReadDxStaticMeshBucketList(c, dwVer, EP7AlphaBucket::Opaque,    outBatches, outBounds)) return false;
            if (!ReadDxStaticMeshBucketList(c, dwVer, EP7AlphaBucket::HardAlpha, outBatches, outBounds)) return false;
            if (!ReadDxStaticMeshBucketList(c, dwVer, EP7AlphaBucket::SoftAlpha, outBatches, outBounds)) return false;
        } else if (dwVer == 0x0102u) {
            if (!ReadDxStaticMeshBucketList(c, dwVer, EP7AlphaBucket::Opaque,      outBatches, outBounds)) return false;
            if (!ReadDxStaticMeshBucketList(c, dwVer, EP7AlphaBucket::HardAlpha,   outBatches, outBounds)) return false;
            if (!ReadDxStaticMeshBucketList(c, dwVer, EP7AlphaBucket::SoftAlpha,   outBatches, outBounds)) return false;
            if (!ReadDxStaticMeshBucketList(c, dwVer, EP7AlphaBucket::SoftAlpha01, outBatches, outBounds)) return false;
            if (!ReadDxStaticMeshBucketList(c, dwVer, EP7AlphaBucket::SoftAlpha02, outBatches, outBounds)) return false;
        } else { // 0x0103, 0x0104, 0x0200
            if (!ReadDxStaticMeshBucketList(c, dwVer, EP7AlphaBucket::Opaque,      outBatches, outBounds)) return false;
            if (!ReadDxStaticMeshBucketList(c, dwVer, EP7AlphaBucket::SoftAlpha,   outBatches, outBounds)) return false;
            if (!ReadDxStaticMeshBucketList(c, dwVer, EP7AlphaBucket::SoftAlpha01, outBatches, outBounds)) return false;
            if (!ReadDxStaticMeshBucketList(c, dwVer, EP7AlphaBucket::SoftAlpha02, outBatches, outBounds)) return false;
            if (!ReadDxStaticMeshBucketList(c, dwVer, EP7AlphaBucket::HardAlpha,   outBatches, outBounds)) return false;
        }
        return true;
    }
    return c.Seek(headerEnd);
}

bool ReadDxLight(Cursor& c, uint32_t lightManVer, std::vector<EP7LightInfo>& outLights) {
    EP7LightInfo li;
    if (lightManVer <= 0x0100u) {
        bool bAffine = false;
        if (!c.ReadBOOL(bAffine)) return false;
        if (bAffine && !c.Skip(36)) return false; // DXAFFINEPARTS
    } else {
        if (!c.Skip(64)) return false; // D3DXMATRIX m_matWorld
    }

    if (!c.ReadFixedAscii(256, li.name)) return false;
    bool bEnable = false;
    EP7Color4 baseDiffuse;
    if (!c.ReadBOOL(bEnable) || !c.ReadColor4(baseDiffuse)) return false;
    li.enabled = bEnable;

    // D3DLIGHTQ (104 bytes)
    uint32_t typeRaw = 1;
    EP7Color4 spec;
    float falloff = 1.0f, theta = 0.0f, phi = 0.0f;
    if (!c.ReadU32(typeRaw) || !c.ReadColor4(li.diffuse) || !c.ReadColor4(spec) ||
        !c.ReadColor4(li.ambient) || !c.ReadVec3(li.position) || !c.ReadVec3(li.direction) ||
        !c.ReadF32(li.range) || !c.ReadF32(falloff) || !c.ReadF32(li.attenuation0) ||
        !c.ReadF32(li.attenuation1) || !c.ReadF32(li.attenuation2) ||
        !c.ReadF32(theta) || !c.ReadF32(phi)) {
        return false;
    }
    li.type = static_cast<EP7LightType>(typeRaw);

    if (lightManVer <= 0x0101u) {
        if (baseDiffuse.a != li.diffuse.a || baseDiffuse.r != li.diffuse.r ||
            baseDiffuse.g != li.diffuse.g || baseDiffuse.b != li.diffuse.b) {
            li.ambient = baseDiffuse;
        }
        if (li.type == EP7LightType::Point) {
            li.diffuse.r = std::min(1.0f, li.diffuse.r * 1.6f);
            li.diffuse.g = std::min(1.0f, li.diffuse.g * 1.6f);
            li.diffuse.b = std::min(1.0f, li.diffuse.b * 1.6f);
            li.ambient = {0.0f, 0.0f, 0.0f, 1.0f};
        }
    }
    li.attenuation0 = 1.0f;
    li.attenuation1 = (li.range > 1e-4f) ? (1.0f / li.range) : 0.01f;
    li.attenuation2 = 0.0f;

    outLights.push_back(std::move(li));

    bool bNext = false;
    if (!c.ReadBOOL(bNext)) return false;
    if (bNext && !ReadDxLight(c, lightManVer, outLights)) return false;
    return true;
}

bool SkipDxLightNode(Cursor& c) {
    if (!c.Skip(24)) return false; // vMax + vMin
    bool bName = false, bLeft = false, bRight = false;
    if (!c.ReadBOOL(bName)) return false;
    if (bName && !c.Skip(256)) return false;
    if (!c.ReadBOOL(bLeft)) return false;
    if (bLeft && !SkipDxLightNode(c)) return false;
    if (!c.ReadBOOL(bRight)) return false;
    if (bRight && !SkipDxLightNode(c)) return false;
    return true;
}

bool ReadDxLightMan(Cursor& c, std::vector<EP7LightInfo>& outLights) {
    uint32_t dwVer = 0;
    bool bLighting = false, bNightAndDay = false, bWhenDayPointOff = false;
    if (!c.ReadU32(dwVer) || !c.ReadBOOL(bLighting) ||
        !c.ReadBOOL(bNightAndDay) || !c.ReadBOOL(bWhenDayPointOff)) {
        return false;
    }

    std::vector<EP7LightInfo> nightDir, noonDir;
    if (!ReadDxLight(c, dwVer, nightDir)) return false;
    if (!ReadDxLight(c, dwVer, noonDir)) return false;

    for (auto& d : noonDir) {
        if (dwVer <= 0x0101u) {
            d.diffuse.r = std::min(1.0f, d.diffuse.r * 1.6f);
            d.diffuse.g = std::min(1.0f, d.diffuse.g * 1.6f);
            d.diffuse.b = std::min(1.0f, d.diffuse.b * 1.6f);
            d.ambient.r = std::min(1.0f, d.ambient.r * 1.2f);
            d.ambient.g = std::min(1.0f, d.ambient.g * 1.2f);
            d.ambient.b = std::min(1.0f, d.ambient.b * 1.2f);
        }
        outLights.push_back(d);
    }

    bool bList = false;
    if (!c.ReadBOOL(bList)) return false;
    if (bList && !ReadDxLight(c, dwVer, outLights)) return false;

    bool bTree = false;
    if (!c.ReadBOOL(bTree)) return false;
    if (bTree && !SkipDxLightNode(c)) return false;
    return true;
}

bool ReadNavigationMesh(Cursor& c, EP7WorldData& world) {
    uint32_t nVerts = 0;
    if (!c.ReadU32(nVerts)) return false;
    if (nVerts > kMaxMeshVerts) return c.Fail("NaviMesh vertex count overflow");
    world.navVertexCount = nVerts;

    if (nVerts > 0) {
        const uint32_t step = std::max<uint32_t>(1u, nVerts / 32u);
        EP7Vec3 sum{0.0f, 0.0f, 0.0f};
        for (uint32_t i = 0; i < nVerts; ++i) {
            EP7Vec3 p;
            if (!c.ReadVec3(p)) return false;
            sum.x += p.x; sum.y += p.y; sum.z += p.z;
            if ((i % step) == 0u) world.navSamplePoints.push_back(p);
        }
        world.recommendedSpawn = {
            sum.x / static_cast<float>(nVerts),
            sum.y / static_cast<float>(nVerts),
            sum.z / static_cast<float>(nVerts)
        };
        world.hasRecommendedSpawn = true;
    }

    uint32_t nCells = 0;
    if (!c.ReadU32(nCells)) return false;
    if (nCells > kMaxListCount) return c.Fail("NaviMesh cell count overflow");
    world.navCellCount = nCells;

    // NavigationCell::LoadFile = 188 bytes per cell
    if (!c.Skip(static_cast<size_t>(nCells) * 188u)) return false;

    for (uint32_t i = 0; i < nCells; ++i) {
        for (int side = 0; side < 3; ++side) {
            bool bExist = false;
            if (!c.ReadBOOL(bExist)) return false;
            if (bExist) {
                uint32_t linkId = 0;
                if (!c.ReadU32(linkId)) return false;
            }
        }
    }
    return true;
}

bool ReadDxLandEff(Cursor& c, std::string& outFile) {
    uint32_t dwVer = 0, dwSize = 0;
    if (!c.ReadU32(dwVer) || !c.ReadU32(dwSize)) return false;
    std::string name;
    if (dwVer == 0x0100u) {
        // LANDEFF_MAXSZ(33) + MAX_PATH(260) + DXAFFINEPARTS(36) = 329
        if (!c.ReadFixedAscii(33, name) || !c.ReadFixedAscii(260, outFile) || !c.Skip(36)) {
            return false;
        }
    } else if (dwVer == 0x0103u || dwVer == 0x0102u || dwVer == 0x0101u) {
        // LANDEFF_MAXSZ(33) + MAX_PATH(260) + D3DXMATRIX(64) = 357 (or +8 in 0x0103)
        if (!c.ReadFixedAscii(33, name) || !c.ReadFixedAscii(260, outFile) || !c.Skip(64)) {
            return false;
        }
        // DXLANDEFF 0x0103 in innerzone_01.Lev/wld writes 8 extra trailing bytes (365 total)
        // when dwSize == 357? Check remaining vs actual struct! Wait: ReadFixedAscii(33)+260+64 = 357 = dwSize!
        if (dwSize > 357u) {
            if (!c.Skip(dwSize - 357u)) return false;
        }
    } else {
        if (!c.Skip(dwSize)) return false;
    }
    return true;
}

bool ReadDxLandGateList(Cursor& c, std::vector<EP7PortalGate>& outGates) {
    while (true) {
        bool bExist = false;
        if (!c.ReadBOOL(bExist)) return false;
        if (!bExist) break;

        uint32_t dwVer = 0, dwSize = 0;
        if (!c.ReadU32(dwVer) || !c.ReadU32(dwSize)) return false;
        const size_t start = c.Pos();
        EP7PortalGate g;
        uint32_t startPNum = 0;

        if (dwVer == 0x0100u) {
            // DxLandGate::PROPERTY_100: m_szName[65] padded to 68 bytes (MSVC align 4)
            if (!c.ReadFixedAscii(68, g.name) ||
                !c.ReadU32(g.gateFlags) || !c.ReadU32(g.gateId) ||
                !c.ReadU16(g.toMapMainId) || !c.ReadU16(g.toMapSubId) ||
                !c.ReadU32(g.toGateId) || !c.Skip(8) ||
                !c.ReadMaxMinAABB(g.bounds) || !c.ReadU32(startPNum)) {
                return false;
            }
        } else if (dwVer == 0x0101u || dwVer == 0x0102u) {
            EP7Vec3 vmin, vmax;
            if (!c.ReadFixedAscii(68, g.name) ||
                !c.ReadU32(g.gateId) || !c.ReadU32(g.gateFlags) || !c.ReadU32(startPNum) ||
                !c.ReadU16(g.toMapMainId) || !c.ReadU16(g.toMapSubId) ||
                !c.ReadU32(g.toGateId) || !c.Skip(8) ||
                !c.ReadVec3(vmin) || !c.ReadVec3(vmax)) {
                return false;
            }
            g.bounds.Expand(vmin);
            g.bounds.Expand(vmax);
            if (dwVer == 0x0102u) {
                const size_t propSize = (dwSize >= 4u) ? (dwSize - 4u - startPNum * 12u) : 120u;
                const size_t consumed = c.Pos() - start;
                if (propSize > consumed && !c.Skip(propSize - consumed)) return false;
            }
        } else {
            if (!c.Skip(dwSize)) return false;
            continue;
        }

        if (startPNum > 1024u) return c.Fail("Gate start point count overflow");
        if (!c.Skip(static_cast<size_t>(startPNum) * 12u)) return false;
        outGates.push_back(std::move(g));
    }
    return true;
}

bool ReadDxFog(Cursor& c, EP7FogInfo& fog) {
    uint32_t dwVer = 0, dwBuffSize = 0;
    if (!c.ReadU32(dwVer) || !c.ReadU32(dwBuffSize)) return false;
    const size_t start = c.Pos();
    if (dwBuffSize > 4096u) return c.Fail("Fog buffer overflow");

    auto unpack = [](uint32_t d3d) -> EP7Color4 {
        return {
            static_cast<float>((d3d >> 16) & 0xFFu) / 255.0f,
            static_cast<float>((d3d >>  8) & 0xFFu) / 255.0f,
            static_cast<float>((d3d      ) & 0xFFu) / 255.0f,
            1.0f
        };
    };

    if (dwVer == 0x0106u || dwVer == 0x0107u || dwVer == 0x0108u) {
        bool bSkyFog = false, bStatic = false;
        uint32_t cCur = 0, cDay = 0, cNight = 0;
        if (!c.ReadBOOL(bSkyFog) || !c.ReadBOOL(bStatic) ||
            !c.ReadF32(fog.fogStart) || !c.ReadF32(fog.fogEnd) ||
            !c.ReadU32(cCur) || !c.ReadU32(cDay) || !c.ReadU32(cNight)) {
            return false;
        }
        fog.enabled = (fog.fogEnd > fog.fogStart && fog.fogEnd > 1.0f);
        fog.skyFogEnable = bSkyFog;
        fog.staticRange = bStatic;
        fog.fogColor = unpack(cCur);
        fog.fogColorDay = unpack(cDay);
        fog.fogColorNight = unpack(cNight);
        // FOG_PROPERTY::Save 0x0108 records sizeof(FOG_PROPERTY)=32 while writing 28 bytes (7 DWORDs).
        if (dwVer == 0x0108u) return c.Seek(start + 28u);
    }

    const size_t consumed = c.Pos() - start;
    if (dwBuffSize > consumed) {
        if (!c.Skip(dwBuffSize - consumed)) return false;
    }
    return true;
}

bool ReadDxSky(Cursor& c, EP7SkyInfo& sky) {
    uint32_t dwVer = 0, dwBuffSize = 0;
    if (!c.ReadU32(dwVer) || !c.ReadU32(dwBuffSize)) return false;
    const size_t start = c.Pos();
    if (dwBuffSize > 4096u) return c.Fail("Sky buffer overflow");
    if (dwVer >= 0x0100u && dwVer <= 0x0103u && dwBuffSize >= 16u) {
        bool bSky = false, bCloud = false;
        if (!c.ReadBOOL(bSky) || !c.ReadBOOL(bCloud) ||
            !c.ReadI32(sky.radioAxis) || !c.ReadF32(sky.axisValue)) {
            return false;
        }
        sky.skyEnable = bSky;
        sky.cloudEnable = bCloud;
    }
    const size_t consumed = c.Pos() - start;
    if (dwBuffSize > consumed && !c.Skip(dwBuffSize - consumed)) return false;
    return true;
}

// Helper functions
std::string BaseFilename(const std::string& path) {
    const size_t p = path.find_last_of("/\\");
    return (p == std::string::npos) ? path : path.substr(p + 1u);
}

void ScanDirectoryRecursive(const std::string& dir,
                            std::unordered_map<std::string, std::string>& outLowerToPath) {
    DIR* d = opendir(dir.c_str());
    if (!d) return;
    struct dirent* ent = nullptr;
    while ((ent = readdir(d)) != nullptr) {
        const char* name = ent->d_name;
        if (std::strcmp(name, ".") == 0 || std::strcmp(name, "..") == 0) continue;
        const std::string full = dir + "/" + name;
        struct stat st{};
        if (stat(full.c_str(), &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            ScanDirectoryRecursive(full, outLowerToPath);
        } else if (S_ISREG(st.st_mode)) {
            outLowerToPath[EP7HashVerify::LowerAscii(name)] = full;
        }
    }
    closedir(d);
}

bool LoadFileBytes(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream ifs(path, std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) return false;
    const std::streamsize sz = ifs.tellg();
    if (sz <= 0) return false;
    ifs.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(sz));
    return static_cast<bool>(ifs.read(reinterpret_cast<char*>(out.data()), sz));
}

} // namespace

EP7WorldData EP7WorldParser::ParseWld(const std::vector<uint8_t>& wldBytes,
                                      const std::vector<uint8_t>& wld0Bytes) {
    EP7WorldData world;
    world.wldFileBytes = wldBytes.size();
    world.wld0FileBytes = wld0Bytes.size();

    const EP7SerialFileView hdr = EP7SerialFile::ReadHeaderOnly(wldBytes);
    if (!hdr.valid) {
        world.error = "Invalid CSerialFile header in .wld: " + hdr.error;
        return world;
    }
    if (hdr.fileType != "LAND.MAN") {
        world.error = "Unexpected .wld fileType: " + hdr.fileType;
        return world;
    }

    Cursor c(wldBytes.data(), wldBytes.size(), EP7SerialFile::kHeaderSize);
    world.version = hdr.version;
    if (world.version < kMinWldVersion || world.version > kMaxWldVersion) {
        world.error = "Unsupported .wld version";
        return world;
    }

    if (!c.ReadU16(world.mapMainId) || !c.ReadU16(world.mapSubId) ||
        !c.ReadFixedAscii(128, world.mapName)) {
        world.error = c.Error();
        return world;
    }

    // SLAND_FILEMARK (dwVer, dwSize, payload)
    uint32_t markVer = 0, markSize = 0;
    if (!c.ReadU32(markVer) || !c.ReadU32(markSize) || !c.Skip(markSize)) {
        world.error = c.Error();
        return world;
    }
    (void)markVer;

    // 1. DxOctree (always present inline; no leading BOOL)
    if (!ReadDxOctree(c, world.terrainBatches, world.octreeNodes, world.worldBounds)) {
        world.error = c.Error();
        return world;
    }

    // 2. DxAnimationMan
    bool bAniMan = false;
    if (!c.ReadBOOL(bAniMan)) {
        world.error = c.Error();
        return world;
    }
    if (bAniMan) {
        if (!ReadDxAnimationMan(c, world.terrainBatches, world.worldBounds)) {
            world.error = c.Error();
            return world;
        }
    }

    // 3. DXPIECEOBJ list (fCurTime + bFileName + bFrame(bFrameName) + matComb)
    while (true) {
        bool bPiece = false;
        if (!c.ReadBOOL(bPiece)) {
            world.error = c.Error();
            return world;
        }
        if (!bPiece) break;

        float curTime = 0.0f;
        if (!c.ReadF32(curTime)) { world.error = c.Error(); return world; }
        (void)curTime;

        EP7PieceInstance inst;
        bool bFile = false;
        if (!c.ReadBOOL(bFile)) { world.error = c.Error(); return world; }
        if (bFile && !c.ReadLenNoNulString(inst.pieceFile)) {
            world.error = c.Error();
            return world;
        }
        bool bFrame = false;
        if (!c.ReadBOOL(bFrame)) { world.error = c.Error(); return world; }
        if (bFrame) {
            bool bFrameName = false;
            if (!c.ReadBOOL(bFrameName)) { world.error = c.Error(); return world; }
            if (bFrameName) {
                std::string frameName;
                if (!c.ReadLenNoNulString(frameName)) { world.error = c.Error(); return world; }
            }
        }
        if (!c.ReadMat4(inst.worldMatrix)) { world.error = c.Error(); return world; }
        inst.fromPieManager = false;
        world.pieceInstances.push_back(std::move(inst));
    }

    // 4. EffectLoadToList
    if (world.version < 0x0110u) {
        if (!ReadCountedEffectList(c)) { world.error = c.Error(); return world; }
    } else {
        if (!ReadCountedEffectList(c) || !ReadCountedEffectList(c) || !ReadCountedEffectList(c)) {
            world.error = c.Error();
            return world;
        }
        if (world.version >= 0x0113u) {
            if (!ReadCountedEffectList(c)) { world.error = c.Error(); return world; }
        }
    }

    // 5. DxLightMan
    // DxLandMan::Load always calls m_LightMan.Load directly (no leading BOOL!)
    if (!ReadDxLightMan(c, world.lights)) {
        world.error = c.Error();
        return world;
    }

    // 6. NavigationMesh
    bool bNavi = false;
    if (!c.ReadBOOL(bNavi)) {
        world.error = c.Error();
        return world;
    }
    if (bNavi) {
        if (!ReadNavigationMesh(c, world)) {
            world.error = c.Error();
            return world;
        }
    }

    // 7. DxSoundMan (always called directly)
    if (!c.Skip(26)) {
        world.error = c.Error();
        return world;
    }

    // 8. CStaticSoundMan
    bool bStaticSound = false;
    if (!c.ReadBOOL(bStaticSound)) {
        world.error = c.Error();
        return world;
    }
    if (bStaticSound) {
        uint16_t v1 = 0, v2 = 0, v3 = 0, cnt = 0;
        if (!c.ReadU16(v1) || !c.ReadU16(v2) || !c.ReadU16(v3) || !c.ReadU16(cnt)) {
            world.error = c.Error();
            return world;
        }
        for (uint16_t i = 0; i < cnt; ++i) {
            std::string sndName;
            if (!c.ReadSerialString(sndName) || !c.Skip(44)) {
                world.error = c.Error();
                return world;
            }
        }
    }

    // 9. DXLANDEFF list (BOOL bListExist -> while(bExist))
    bool bLandEffList = false;
    if (!c.ReadBOOL(bLandEffList)) { world.error = c.Error(); return world; }
    if (bLandEffList) {
        while (true) {
            std::string effFile;
            if (!ReadDxLandEff(c, effFile)) { world.error = c.Error(); return world; }
            if (!effFile.empty()) world.landEffects.push_back(std::move(effFile));
            bool bNextEff = false;
            if (!c.ReadBOOL(bNextEff)) { world.error = c.Error(); return world; }
            if (!bNextEff) break;
        }
    }

    // 10. DxCamAniMan::Load (reads DWORD dwNumCameraAni, NOT a trailing BOOL sentinel)
    bool bCamAni = false;
    if (!c.ReadBOOL(bCamAni)) { world.error = c.Error(); return world; }
    if (bCamAni) {
        uint32_t camCount = 0;
        if (!c.ReadU32(camCount)) { world.error = c.Error(); return world; }
        if (camCount > 1000u) { world.error = "DxCamAniMan count overflow"; return world; }
        for (uint32_t ci = 0; ci < camCount; ++ci) {
            uint32_t camVer = 0, camSize = 0;
            if (!c.ReadU32(camVer) || !c.ReadU32(camSize) || !c.Skip(camSize)) {
                world.error = c.Error();
                return world;
            }
            (void)camVer;
        }
    }

    // 11. DXLANDSKINOBJ list (BOOL bListExist -> while(bExist))
    bool bSkinList = false;
    if (!c.ReadBOOL(bSkinList)) { world.error = c.Error(); return world; }
    if (bSkinList) {
        while (true) {
            uint32_t skVer = 0, skSize = 0;
            if (!c.ReadU32(skVer) || !c.ReadU32(skSize)) { world.error = c.Error(); return world; }
            const size_t skEnd = c.Pos() + skSize;
            if (skEnd > c.Size()) { world.error = "DXLANDSKINOBJ overflows .wld"; return world; }
            std::string skName, skFile;
            if (skSize >= 293u && c.ReadFixedAscii(33, skName) && c.ReadFixedAscii(260, skFile)) {
                if (!skFile.empty()) world.skinObjects.push_back(std::move(skFile));
            }
            (void)skVer;
            if (!c.Seek(skEnd)) { world.error = c.Error(); return world; }
            bool bNextSkin = false;
            if (!c.ReadBOOL(bNextSkin)) { world.error = c.Error(); return world; }
            if (!bNextSkin) break;
        }
    }

    // 12. DxLandGateMan
    bool bGateMan = false;
    if (!c.ReadBOOL(bGateMan)) { world.error = c.Error(); return world; }
    if (bGateMan) {
        if (!ReadDxLandGateList(c, world.gates)) {
            world.error = c.Error();
            return world;
        }
    }

    // 13. DxWeatherMan (4 bUse flags: Rain, Snow, Cloud, trailing sentinel)
    bool bWeather = false;
    if (!c.ReadBOOL(bWeather)) { world.error = c.Error(); return world; }
    if (bWeather) {
        uint32_t wVer = 0, wSize = 0;
        if (!c.ReadU32(wVer) || !c.ReadU32(wSize) || !c.Skip(wSize)) {
            world.error = c.Error();
            return world;
        }
        if (wVer >= 0x0101u) {
            for (int part = 0; part < 4; ++part) {
                bool bUse = false;
                if (!c.ReadBOOL(bUse)) { world.error = c.Error(); return world; }
                if (!bUse || part == 3) break;
                uint32_t v = 0, s = 0;
                if (!c.ReadU32(v) || !c.ReadU32(s) || !c.Skip(s)) {
                    world.error = c.Error();
                    return world;
                }
            }
        }
    }

    // 14. SBGMDATA
    bool bBgm = false;
    if (!c.ReadBOOL(bBgm)) { world.error = c.Error(); return world; }
    if (bBgm) {
        bool bUse = false;
        if (!c.ReadBOOL(bUse)) { world.error = c.Error(); return world; }
        if (bUse) {
            if (!c.ReadSerialString(world.bgmFile)) { world.error = c.Error(); return world; }
        }
    }

    // 15. CollisionMap (BOOL bExist + DWORD dwBlockSize -> Skip(dwBlockSize))
    bool bColl = false;
    uint32_t collSize = 0;
    if (!c.ReadBOOL(bColl) || !c.ReadU32(collSize) || !c.Skip(collSize)) {
        world.error = c.Error();
        return world;
    }

    // 16. FOG_PROPERTY (BOOL bExist), SKY_PROPERTY (BOOL bExist)
    bool hasMore = true;
    if (c.Remaining() >= 4) {
        bool bFog = false;
        if (!c.ReadBOOL(bFog)) { world.error = c.Error(); return world; }
        if (bFog) {
            if (!ReadDxFog(c, world.fog)) { world.error = c.Error(); return world; }
        } else {
            hasMore = false;
        }
    }
    if (hasMore && c.Remaining() >= 4) {
        bool bSky = false;
        if (!c.ReadBOOL(bSky)) { world.error = c.Error(); return world; }
        if (bSky) {
            if (!ReadDxSky(c, world.sky)) { world.error = c.Error(); return world; }
        } else {
            hasMore = false;
        }
    }

    // 17. Version >= 0x0111: StaticMesh (flag BOOL) + PieceManager.LoadWLD (BOOL)
    if (hasMore && world.version >= 0x0111u && c.Remaining() >= 4) {
        bool bStaticMesh = false;
        if (!c.ReadBOOL(bStaticMesh)) { world.error = c.Error(); return world; }
        if (!bStaticMesh) hasMore = false;
        if (hasMore && c.Remaining() >= 4) {
            bool bPieceMgr = false;
            if (!c.ReadBOOL(bPieceMgr)) { world.error = c.Error(); return world; }
            if (bPieceMgr) {
                // DxPieceManager::LoadWLD: DWORD dwPieceTypes, then serial map -> DxPieceAABB::Load (dwPieceNUM + NSPIECETREE)
                uint32_t pieceMapCount = 0;
                if (!c.ReadU32(pieceMapCount)) { world.error = c.Error(); return world; }
                if (pieceMapCount > 10000u) { world.error = "PieceManager count overflow"; return world; }
                std::function<bool(const std::string&)> readPieceNode = [&](const std::string& pieName) -> bool {
                    bool bNodeExist = false;
                    if (!c.ReadBOOL(bNodeExist)) return false;
                    if (!bNodeExist) return true;
                    EP7AABB nodeBox;
                    uint32_t instCount = 0;
                    if (!c.ReadMaxMinAABB(nodeBox) || !c.ReadU32(instCount)) return false;
                    if (instCount > 50000u) return c.Fail("DxPieceNode count overflow");
                    for (uint32_t ii = 0; ii < instCount; ++ii) {
                        EP7PieceInstance inst;
                        inst.pieceFile = pieName;
                        inst.fromPieManager = true;
                        if (!c.ReadMaxMinAABB(inst.worldBounds) || !c.ReadMat4(inst.worldMatrix)) {
                            return false;
                        }
                        inst.center = inst.worldBounds.Center();
                        inst.boundingRadius = inst.worldBounds.BoundingRadius();
                        world.pieceInstances.push_back(std::move(inst));
                    }
                    return readPieceNode(pieName) && readPieceNode(pieName);
                };
                for (uint32_t pi = 0; pi < pieceMapCount; ++pi) {
                    std::string pieName;
                    uint32_t totalPieces = 0;
                    if (!c.ReadSerialString(pieName) || !c.ReadU32(totalPieces)) {
                        world.error = c.Error();
                        return world;
                    }
                    (void)totalPieces;
                    if (!readPieceNode(pieName)) {
                        world.error = c.Error();
                        return world;
                    }
                }
            }
        }
    }
    // Optional trailing 4-byte 0x00000000 footer written in 0x0108 (login.wld)
    if (c.Remaining() == 4) {
        uint32_t tailZero = 0;
        std::memcpy(&tailZero, c.Ptr(), 4);
        if (tailZero == 0u) c.Skip(4);
    }

    world.wldBytesConsumed = c.Pos();

    // Parse companion .wld0 static mesh if supplied
    if (!wld0Bytes.empty()) {
        const EP7SerialFileView w0Hdr = EP7SerialFile::ReadHeaderOnly(wld0Bytes);
        const size_t w0Start = w0Hdr.valid ? EP7SerialFile::kHeaderSize : 0u;
        Cursor w0c(wld0Bytes.data(), wld0Bytes.size(), w0Start);
        if (!ReadDxStaticMesh(w0c, world.terrainBatches, world.worldBounds)) {
            world.error = "Failed parsing .wld0: " + w0c.Error();
            return world;
        }
        world.hasWld0 = true;
        world.wld0BytesConsumed = w0c.Pos();
    }

    for (const auto& b : world.terrainBatches) {
        world.worldBounds.ExpandAABB(b.bounds);
    }
    if (!world.hasRecommendedSpawn && world.worldBounds.valid) {
        world.recommendedSpawn = world.worldBounds.Center();
        world.hasRecommendedSpawn = true;
    }

    RebuildTextureRefs(world);
    world.valid = true;
    return world;
}

bool EP7WorldParser::ParsePieceFile(const std::string& filename,
                                    const std::vector<uint8_t>& bytes,
                                    EP7PieceAsset& outPiece,
                                    std::string* errorOut) {
    outPiece = EP7PieceAsset{};
    outPiece.filename = BaseFilename(filename);

    const EP7SerialFileView hdr = EP7SerialFile::ReadHeaderOnly(bytes);
    const size_t startOffset = hdr.valid ? EP7SerialFile::kHeaderSize : 0u;
    Cursor c(bytes.data(), bytes.size(), startOffset);

    const std::string lower = EP7HashVerify::LowerAscii(filename);
    const bool looksPie = lower.size() >= 4 && lower.compare(lower.size() - 4, 4, ".pie") == 0;
    outPiece.isPie = looksPie;

    if (looksPie) {
        // DxPieceEdit::Load (external/recovery/phase3i/source/DxPiece.cpp / DxPiece2.cpp)
        uint32_t dwVer = 0, dwSize = 0;
        if (!c.ReadU32(dwVer) || !c.ReadU32(dwSize)) {
            outPiece.error = c.Error();
            if (errorOut) *errorOut = outPiece.error;
            return false;
        }
        const size_t hdrEnd = c.Pos() + dwSize;
        if (hdrEnd > c.Size()) {
            outPiece.error = "DxPieceEdit header block exceeds file size";
            if (errorOut) *errorOut = outPiece.error;
            return false;
        }
        if (!c.ReadMaxMinAABB(outPiece.localBounds)) {
            outPiece.error = c.Error();
            if (errorOut) *errorOut = outPiece.error;
            return false;
        }
        if (!c.Seek(hdrEnd)) {
            outPiece.error = c.Error();
            if (errorOut) *errorOut = outPiece.error;
            return false;
        }

        // BOOL bStaticAniFrame -> DxStaticAniFrame::Load
        bool bStaticAni = false;
        if (!c.ReadBOOL(bStaticAni)) {
            outPiece.error = c.Error();
            if (errorOut) *errorOut = outPiece.error;
            return false;
        }
        if (bStaticAni) {
            uint32_t aniVer = 0, aniSize = 0;
            if (!c.ReadU32(aniVer) || !c.ReadU32(aniSize)) {
                outPiece.error = c.Error();
                if (errorOut) *errorOut = outPiece.error;
                return false;
            }
            const size_t aniEnd = c.Pos() + aniSize;
            if (aniEnd > c.Size()) {
                outPiece.error = "DxStaticAniFrame block exceeds file size";
                if (errorOut) *errorOut = outPiece.error;
                return false;
            }
            bool bStaticMesh = false;
            if (!c.ReadBOOL(bStaticMesh)) {
                outPiece.error = c.Error();
                if (errorOut) *errorOut = outPiece.error;
                return false;
            }
            if (bStaticMesh) {
                if (!ReadDxStaticMesh(c, outPiece.batches, outPiece.localBounds)) {
                    outPiece.error = c.Error();
                    if (errorOut) *errorOut = outPiece.error;
                    return false;
                }
            }
            if (!c.Seek(aniEnd)) {
                outPiece.error = c.Error();
                if (errorOut) *errorOut = outPiece.error;
                return false;
            }
        }
        // Trailing DxPieceEff / DxPieceTexEff / bDummy
        if (c.Remaining() >= 4) {
            c.Seek(c.Size());
        }
    } else {
        // DXREPLACEPIECE::Load (.pis)
        if (!c.ReadMaxMinAABB(outPiece.localBounds)) {
            outPiece.error = c.Error();
            if (errorOut) *errorOut = outPiece.error;
            return false;
        }
        // Root DxFrame tree (BOOL bFrameRoot already handled inside ReadDxFrameTree)
        if (!ReadDxFrameTree(c, true, outPiece.batches, outPiece.localBounds)) {
            outPiece.error = c.Error();
            if (errorOut) *errorOut = outPiece.error;
            return false;
        }
        // DxAnimationMan
        bool bAniMan = false;
        if (!c.ReadBOOL(bAniMan)) {
            outPiece.error = c.Error();
            if (errorOut) *errorOut = outPiece.error;
            return false;
        }
        if (bAniMan && !ReadDxAnimationMan(c, outPiece.batches, outPiece.localBounds)) {
            outPiece.error = c.Error();
            if (errorOut) *errorOut = outPiece.error;
            return false;
        }
        bool bRendAni = false;
        if (!c.ReadBOOL(bRendAni)) {
            outPiece.error = c.Error();
            if (errorOut) *errorOut = outPiece.error;
            return false;
        }
        if (!ReadCountedEffectList(c)) {
            outPiece.error = c.Error();
            if (errorOut) *errorOut = outPiece.error;
            return false;
        }
        // DXLANDEFF list inside .pis (BOOL bListExist -> while(bExist) { Load; bExist; })
        bool bLandEffList = false;
        if (!c.ReadBOOL(bLandEffList)) {
            outPiece.error = c.Error();
            if (errorOut) *errorOut = outPiece.error;
            return false;
        }
        if (bLandEffList) {
            while (true) {
                std::string effFile;
                if (!ReadDxLandEff(c, effFile)) {
                    outPiece.error = c.Error();
                    if (errorOut) *errorOut = outPiece.error;
                    return false;
                }
                if (!effFile.empty()) outPiece.landEffects.push_back(effFile);
                bool bNextEff = false;
                if (!c.ReadBOOL(bNextEff)) {
                    outPiece.error = c.Error();
                    if (errorOut) *errorOut = outPiece.error;
                    return false;
                }
                if (!bNextEff) break;
            }
        }
        // Optional trailing DxStaticMesh in newer .pis
        if (c.Remaining() >= 4) {
            bool bStatic = false;
            if (!c.ReadBOOL(bStatic)) {
                outPiece.error = c.Error();
                if (errorOut) *errorOut = outPiece.error;
                return false;
            }
            if (bStatic && !ReadDxStaticMesh(c, outPiece.batches, outPiece.localBounds)) {
                outPiece.error = c.Error();
                if (errorOut) *errorOut = outPiece.error;
                return false;
            }
        }
    }

    outPiece.bytesConsumed = c.Pos();
    outPiece.valid = true;
    return true;
}

EP7LevelFullData EP7WorldParser::ParseFullLevel(const std::vector<uint8_t>& levBytes,
                                                EP7ValidationMode mode) {
    EP7LevelFullData out;
    out.head = EP7LevelHeaderParser::Parse(levBytes, mode);
    if (!out.head.valid) {
        out.error = out.head.error;
        return out;
    }

    const EP7SerialFileView view = EP7SerialFile::Read(
        levBytes, EP7Serial::EP7ByteCryptMode::LevelNew);
    if (!view.valid) {
        out.error = view.error;
        return out;
    }

    if (view.version != 0x0200u) {
        out.error = "Full level parser requires LOAD_200 (0x0200)";
        return out;
    }

    Cursor c(view.payload.data(), view.payload.size(), 0);

    // SLEVEL_HEAD (headVer + headSize + headBytes)
    uint32_t headVer = 0, headSize = 0;
    if (!c.ReadU32(headVer) || !c.ReadU32(headSize) || !c.Skip(headSize)) {
        out.error = "Truncated SLEVEL_HEAD in .Lev";
        return out;
    }
    (void)headVer;

    // SLEVEL_REQUIRE
    uint32_t reqVer = 0, reqSize = 0;
    if (!c.ReadU32(reqVer) || !c.ReadU32(reqSize) || !c.Skip(reqSize)) {
        out.error = "Truncated SLEVEL_REQUIRE in .Lev";
        return out;
    }
    (void)reqVer;

    // 1. MobSchedule block (DWORD dwBLOCKSIZE -> GLMobScheduleMan)
    uint32_t mobBlockSize = 0;
    if (!c.ReadU32(mobBlockSize)) { out.error = c.Error(); return out; }
    const size_t mobBlockEnd = c.Pos() + mobBlockSize;
    if (mobBlockEnd > c.Size()) { out.error = "MobSchedule block overflows .Lev"; return out; }
    if (mobBlockSize >= 12u) {
        uint32_t mobManVer = 0, mobManSize = 0;
        bool bMobExist = false;
        if (!c.ReadU32(mobManVer) || !c.ReadU32(mobManSize) || !c.ReadBOOL(bMobExist)) {
            out.error = c.Error();
            return out;
        }
        (void)mobManVer; (void)mobManSize;
        while (bMobExist && c.Pos() + 8 <= mobBlockEnd) {
            uint32_t mVer = 0, mSize = 0;
            if (!c.ReadU32(mVer) || !c.ReadU32(mSize)) { out.error = c.Error(); return out; }
            const size_t endMob = c.Pos() + mSize;
            if (endMob > mobBlockEnd) { out.error = "MobSchedule entry overflows block"; return out; }
            if (mVer >= 0x0107u && mSize >= 116u) {
                EP7MobSpawn mob;
                uint8_t dow[7]{};
                int32_t rHour = 0, rMin = 0;
                if (c.ReadU16(mob.crowMainId) && c.ReadU16(mob.crowSubId) &&
                    c.ReadFixedAscii(64, mob.name) &&
                    c.Check(7)) {
                    std::memcpy(dow, c.Ptr(), 7);
                    c.Skip(7);
                    if (c.ReadF32(mob.regenTime) && c.ReadI32(rHour) && c.ReadI32(rMin) &&
                        c.Skip(12) && c.ReadVec3(mob.position)) { // DXAFFINEPARTS starts with vTrans (3*4=12? No: bUseScale/trans — DXAFFINEPARTS vTrans at offset +12)
                        out.mobSpawns.push_back(std::move(mob));
                    }
                }
            }
            if (!c.Seek(endMob)) { out.error = c.Error(); return out; }
            if (!c.ReadBOOL(bMobExist)) { out.error = c.Error(); return out; }
        }
    }
    if (!c.Seek(mobBlockEnd)) { out.error = c.Error(); return out; }

    // 2. DxLandGateMan (DWORD dwBLOCKSIZE -> DxLandGateMan::Load)
    uint32_t gateBlockSize = 0;
    if (!c.ReadU32(gateBlockSize)) { out.error = c.Error(); return out; }
    const size_t gateBlockEnd = c.Pos() + gateBlockSize;
    if (gateBlockEnd > c.Size()) { out.error = "Gate block overflows .Lev"; return out; }
    if (gateBlockSize >= 4u) {
        if (!ReadDxLandGateList(c, out.gates)) { out.error = c.Error(); return out; }
    }
    if (!c.Seek(gateBlockEnd)) { out.error = c.Error(); return out; }

    // 3. DXLANDEFF block (BOOL bLOAD + DWORD dwBLOCKSIZE)
    bool bEffLoad = false;
    uint32_t effBlockSize = 0;
    if (!c.ReadBOOL(bEffLoad) || !c.ReadU32(effBlockSize)) { out.error = c.Error(); return out; }
    const size_t effBlockEnd = c.Pos() + effBlockSize;
    if (effBlockEnd > c.Size()) { out.error = "LandEff block overflows .Lev"; return out; }
    if (bEffLoad) {
        while (c.Pos() < effBlockEnd) {
            std::string effFile;
            if (!ReadDxLandEff(c, effFile)) { out.error = c.Error(); return out; }
            if (!effFile.empty()) out.landEffects.push_back(std::move(effFile));
            bool bMore = false;
            if (!c.ReadBOOL(bMore)) { out.error = c.Error(); return out; }
            if (!bMore) break;
        }
    }
    if (!c.Seek(effBlockEnd)) { out.error = c.Error(); return out; }

    // 4. GLLANDMARK block (BOOL bLOAD + DWORD dwBLOCKSIZE)
    bool bLmLoad = false;
    uint32_t lmBlockSize = 0;
    if (!c.ReadBOOL(bLmLoad) || !c.ReadU32(lmBlockSize)) { out.error = c.Error(); return out; }
    const size_t lmBlockEnd = c.Pos() + lmBlockSize;
    if (lmBlockEnd > c.Size()) { out.error = "LandMark block overflows .Lev"; return out; }
    if (bLmLoad) {
        while (c.Pos() < lmBlockEnd) {
            uint32_t verLm = 0, szLm = 0;
            if (!c.ReadU32(verLm) || !c.ReadU32(szLm) || !c.Skip(szLm)) {
                out.error = c.Error();
                return out;
            }
            ++out.landMarkCount;
            bool bMore = false;
            if (!c.ReadBOOL(bMore)) { out.error = c.Error(); return out; }
            if (!bMore) break;
        }
    }
    if (!c.Seek(lmBlockEnd)) { out.error = c.Error(); return out; }

    // 5. SLEVEL_ETC_FUNC
    if (c.Remaining() >= 8) {
        uint32_t etcVer = 0, etcSize = 0;
        if (!c.ReadU32(etcVer) || !c.ReadU32(etcSize) || !c.Skip(etcSize)) {
            out.error = c.Error();
            return out;
        }
        (void)etcVer;
    }

    out.bytesConsumed = EP7SerialFile::kHeaderSize + c.Pos();
    out.valid = (out.bytesConsumed == levBytes.size());
    if (!out.valid) {
        out.error = "Trailing bytes in .Lev";
    }
    return out;
}

void EP7WorldParser::ResolveAndAttachPieces(EP7WorldData& world, const std::string& assetRoot) {
    std::unordered_map<std::string, std::string> lowerToPath;
    if (!assetRoot.empty()) {
        ScanDirectoryRecursive(assetRoot, lowerToPath);
    }

    std::unordered_map<std::string, int32_t> loadedIndexByLower;
    std::unordered_set<std::string> missingSeen;
    world.missingPieceFiles.clear();

    for (auto& inst : world.pieceInstances) {
        const std::string key = EP7HashVerify::LowerAscii(BaseFilename(inst.pieceFile));
        if (key.empty()) continue;

        auto foundIdx = loadedIndexByLower.find(key);
        if (foundIdx == loadedIndexByLower.end()) {
            auto pathIt = lowerToPath.find(key);
            if (pathIt == lowerToPath.end()) {
                if (missingSeen.insert(inst.pieceFile).second) {
                    world.missingPieceFiles.push_back(inst.pieceFile);
                }
                continue;
            }
            std::vector<uint8_t> bytes;
            if (!LoadFileBytes(pathIt->second, bytes)) {
                if (missingSeen.insert(inst.pieceFile).second) {
                    world.missingPieceFiles.push_back(inst.pieceFile);
                }
                continue;
            }
            EP7PieceAsset piece;
            std::string err;
            if (!ParsePieceFile(pathIt->second, bytes, piece, &err)) {
                if (missingSeen.insert(inst.pieceFile).second) {
                    world.missingPieceFiles.push_back(inst.pieceFile);
                }
                continue;
            }
            const int32_t idx = static_cast<int32_t>(world.pieceAssets.size());
            world.pieceAssets.push_back(std::move(piece));
            loadedIndexByLower[key] = idx;
            inst.assetIndex = idx;
        } else {
            inst.assetIndex = foundIdx->second;
        }

        const EP7PieceAsset& piece = world.pieceAssets[static_cast<size_t>(inst.assetIndex)];
        if (piece.localBounds.valid && !inst.worldBounds.valid) {
            inst.worldBounds = EP7AABB::Transform(inst.worldMatrix, piece.localBounds);
            inst.center = inst.worldBounds.Center();
            inst.boundingRadius = inst.worldBounds.BoundingRadius();
        }
        world.worldBounds.ExpandAABB(inst.worldBounds);
    }

    RebuildTextureRefs(world);
}

void EP7WorldParser::RebuildTextureRefs(EP7WorldData& world) {
    std::unordered_set<std::string> seenLower;
    world.textureRefs.clear();

    auto addTex = [&](const std::string& name) {
        const std::string base = BaseFilename(name);
        if (base.empty()) return;
        const std::string lower = EP7HashVerify::LowerAscii(base);
        if (seenLower.insert(lower).second) {
            world.textureRefs.push_back(base);
        }
    };

    for (const auto& b : world.terrainBatches) addTex(b.textureName);
    for (const auto& p : world.pieceAssets) {
        for (const auto& b : p.batches) addTex(b.textureName);
    }
    if (world.sky.skyEnable) addTex("sky_001.dds");
    if (world.sky.cloudEnable) addTex("dark_sky3.tga");
}

} // namespace RANMobile
