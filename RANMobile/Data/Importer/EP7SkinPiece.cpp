#include "EP7SkinPiece.h"

#include <cstring>

namespace RANMobile {
namespace {

// SVERTEXINFLU::VERSION — enginelib/Meshs/DxSkinMeshContainer9.cpp:36 and
// enginelib/Meshs/SMeshContainer.cpp:29, both of which define
//     const DWORD SVERTEXINFLU::VERSION = 0x0101;
// SVERTEXINFLU::SaveFile() writes that same constant as the record tag, and
// SVERTEXINFLU::LoadFile() compares the tag it reads against it.  This is the
// version every trace-point record in an authentic .cps carries; using any other
// value makes LoadFile take the `SetOffSet(GetfTell() + dwSize)` branch for every
// trace and desynchronises the stream.
constexpr uint32_t kVertexInfluenceVersion = 0x0101;

// SANITY LIMITS — not present in the original (which trusts the file). We impose upper
// bounds so a malformed asset cannot trigger multi-gigabyte allocations.
constexpr uint32_t kMaxPathString = 1024;        // MAX_PATH is 260 in Win32; be generous
constexpr uint32_t kMaxMaterials = 64;
constexpr uint32_t kMaxTraces = 512;
constexpr uint32_t kMaxEffects = 256;

bool ReadPathString(ep7byte::Stream& s, std::string& out, const char* what) {
    uint32_t len = 0;
    if (!s.ReadU32(len)) return false;
    if (len == 0 || len > kMaxPathString) {
        s.Fail(std::string("invalid ") + what + " length");
        return false;
    }
    return s.ReadFixedString(len, out);
}

// SVERTEXINFLU::LoadFile (DxSkinMeshContainer9.cpp:94)
//   DWORD dwVer, DWORD dwSize;  if (dwVer == VERSION) { index, vector, normal, numBone,
//   bones[numBone], weights[numBone] } else SetOffSet(GetfTell() + dwSize).
bool ReadVertexInfluence(ep7byte::Stream& s, EP7VertexInfluence& out) {
    uint32_t version = 0;
    uint32_t blockSize = 0;
    if (!s.ReadU32(version) || !s.ReadU32(blockSize)) return false;

    if (version != kVertexInfluenceVersion) {
        // Original behaviour: skip the unknown block.
        out.valid = false;
        return s.Skip(blockSize);
    }

    if (!s.ReadU32(out.index)) return false;
    if (!s.ReadF32(out.position[0]) || !s.ReadF32(out.position[1]) || !s.ReadF32(out.position[2])) return false;
    if (!s.ReadF32(out.normal[0]) || !s.ReadF32(out.normal[1]) || !s.ReadF32(out.normal[2])) return false;

    uint32_t boneCount = 0;
    if (!s.ReadU32(boneCount)) return false;

    // SetBoneNum() asserts m_dwNumBone <= MAX_BONE; COL_ERR passes through.
    if (boneCount != EP7VertexInfluence::kColErr && boneCount > EP7VertexInfluence::kMaxBone) {
        s.Fail("SVERTEXINFLU bone count exceeds MAX_BONE (6)");
        return false;
    }
    out.boneCount = boneCount;

    if (boneCount > 0 && boneCount != EP7VertexInfluence::kColErr) {
        for (uint32_t i = 0; i < boneCount; ++i) {
            if (!s.ReadU32(out.bones[i])) return false;
        }
        for (uint32_t i = 0; i < boneCount; ++i) {
            if (!s.ReadF32(out.weights[i])) return false;
        }
    }
    out.valid = true;
    return true;
}

}  // namespace

EP7SkinPieceInfo EP7SkinPieceReader::ParseFromBytes(const std::vector<uint8_t>& bytes, bool compByteEncoded) {
    ep7byte::Stream stream;
    stream.Attach(bytes, compByteEncoded);

    // GLOGIC::openfile_basestream -> CSerialFile/CSerialMemory::OpenFile ->
    // ReadFileType(): the 128-byte file-type block and the version DWORD are consumed
    // before any payload byte is read. Strip them here so the caller can pass raw bytes.
    if (bytes.size() >= ep7byte::Stream::kHeaderSize) {
        std::string fileType;
        uint32_t fileId = 0;
        stream.ReadFileType(fileType, fileId);
    }
    return Parse(stream);
}

EP7SkinPieceInfo EP7SkinPieceReader::Parse(ep7byte::Stream& s) {
    EP7SkinPieceInfo info;

    if (!s.IsAttached()) {
        info.error = "empty .cps buffer";
        return info;
    }

    uint32_t version = 0;
    if (!s.ReadU32(version)) {
        info.error = "truncated .cps version";
        return info;
    }
    info.fileVersion = version;

    if (version != kVersion107 && version != kVersion106 && version != kVersion105 && version != kVersion104) {
        info.error = "unsupported .cps version (expected 0x0104..0x0107)";
        return info;
    }

    if (!s.ReadU32(info.refCount)) {
        info.error = "truncated .cps ref count";
        return info;
    }

    if (!ReadPathString(s, info.xFileName, "mesh path")) {
        info.error = "invalid .cps mesh path";
        return info;
    }
    if (!ReadPathString(s, info.skeletonFile, "skeleton path")) {
        info.error = "invalid .cps skeleton path";
        return info;
    }

    // 0x0106/0x0107 write weaponWhereBack right after the piece type; older versions do not.
    const bool hasWeaponSlot = (version >= kVersion106);

    uint32_t rawType = 0;
    if (!s.ReadU32(rawType)) {
        info.error = "truncated .cps piece type";
        return info;
    }
    if (rawType >= static_cast<uint32_t>(EP7PieceType::Size)) {
        info.error = ".cps piece type out of range";
        return info;
    }
    info.pieceType = static_cast<EP7PieceType>(rawType);

    if (hasWeaponSlot) {
        uint32_t slot = 0;
        if (!s.ReadU32(slot)) {
            info.error = "truncated .cps weapon slot";
            return info;
        }
        // EMPEACE_WEAPON_SIZE == 3; the original casts without checking.
        if (slot >= 3) {
            info.error = ".cps weapon-back slot out of range";
            return info;
        }
        info.weaponWhereBack = static_cast<EP7WeaponBackSlot>(slot);
        info.hasWeaponWhereBack = true;
    }

    if (!ReadPathString(s, info.meshName, "mesh name")) {
        info.error = "invalid .cps mesh name";
        return info;
    }

    uint32_t materialCount = 0;
    if (!s.ReadU32(materialCount)) {
        info.error = "truncated .cps material count";
        return info;
    }
    if (materialCount > kMaxMaterials) {
        info.error = ".cps material count exceeds sanity limit";
        return info;
    }
    info.materials.reserve(materialCount);
    for (uint32_t i = 0; i < materialCount; ++i) {
        EP7MaterialPiece mat;
        uint32_t matVersion = 0;
        uint32_t matBlock = 0;
        // SMATERIAL_PIECE::LoadFile reads both header DWORDs unconditionally:
        //     SFile >> dwVERSION;  SFile >> dwBLOCKSIZE;
        // dwBLOCKSIZE is the byte count of the record body that follows, back-patched by
        // CSerialFile::EndBlock() (SaveFile wraps the body in BeginBlock/EndBlock).
        if (!s.ReadU32(matVersion) || !s.ReadU32(matBlock)) {
            info.error = "truncated material block header";
            return info;
        }
        if (matVersion != EP7MaterialPiece::kVersion) {
            // Original: SFile.SetOffSet(SFile.GetfTell() + dwBLOCKSIZE).
            if (!s.Skip(matBlock)) {
                info.error = "material skip past end of buffer";
                return info;
            }
            continue;
        }
        uint32_t texLen = 0;
        if (!s.ReadU32(texLen)) {
            info.error = "truncated material texture length";
            return info;
        }
        if (texLen > kMaxPathString) {
            info.error = "material texture name exceeds sanity limit";
            return info;
        }
        if (texLen > 0) {
            if (!s.ReadFixedString(texLen, mat.texture)) {
                info.error = "truncated material texture name";
                return info;
            }
        }
        if (!s.ReadU32(mat.flags) || !s.ReadU32(mat.zBias)) {
            info.error = "truncated material flags";
            return info;
        }
        // SMATERIAL_PIECE::LoadFile: m_fZBias = (float)m_dwZBias * 0.00001f
        mat.zBiasScaled = static_cast<float>(mat.zBias) * 0.00001f;
        info.materials.push_back(std::move(mat));
    }

    uint32_t traceCount = 0;
    if (!s.ReadU32(traceCount)) {
        info.error = "truncated trace count";
        return info;
    }
    if (traceCount > kMaxTraces) {
        info.error = "trace count exceeds sanity limit";
        return info;
    }
    info.traces.reserve(traceCount);
    for (uint32_t i = 0; i < traceCount; ++i) {
        EP7TracePoint trace;
        if (!s.ReadString(trace.name, kMaxPathString)) {
            info.error = "truncated trace name";
            return info;
        }
        if (!ReadVertexInfluence(s, trace.influence)) {
            info.error = "truncated trace vertex influence";
            return info;
        }
        info.traces.push_back(std::move(trace));
    }

    // Effect list: the original constructs a DxEffChar per TypeID and lets the effect
    // read its own payload.  We cannot skip an unknown effect payload safely, so we
    // record the count and stop.  Data after this point (including m_dwFlag) is left
    // unparsed rather than guessed.
    uint32_t effectCount = 0;
    if (!s.ReadU32(effectCount)) {
        info.error = "truncated effect count";
        return info;
    }
    info.effectCount = effectCount;
    info.effectsPresent = (effectCount != 0);
    if (effectCount > kMaxEffects) {
        info.error = "effect count exceeds sanity limit";
        return info;
    }

    if (effectCount != 0) {
        // Stop here: payload not parsed. Piece metadata above is complete and verified.
        info.valid = true;
        return info;
    }

    if (!s.ReadU32(info.flag)) {
        info.error = "truncated piece flag";
        return info;
    }

    info.valid = true;
    return info;
}

}  // namespace RANMobile
