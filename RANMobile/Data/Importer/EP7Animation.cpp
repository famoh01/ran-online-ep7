#include "EP7Animation.h"

#include <cstdio>

#include <cmath>
#include <cstring>

namespace RANMobile {
namespace {

constexpr uint32_t kMaxBoneTracks = 4096;
constexpr uint32_t kMaxKeysPerTrack = 200000;
constexpr uint32_t kMaxNameLen = 4096;
// B16 (SANIMCONINFO 0x0108..0x0115) sanity caps, in the same spirit as the caps above:
// they bound declared element counts so a corrupt count can never multiply into an
// oversized allocation or a runaway loop. A 4 KiB-bounded effect list / AniScale map is
// far above anything the authentic writers emit (the engine builds these from small
// fixed editor structures).
constexpr uint32_t kMaxCfgEffects = 4096;       // SANIMCONINFO::m_listEffAni entries
constexpr uint32_t kMaxAniScaleEntries = 4096;  // DxAniScale::m_mapAniScaleMain entries
// sizeof(SChaSoundData) on the MSVC x86 build: int(4) + char[10][32](320) + BOOL[10](40)
// + WORD[11](22) = 386 rounded up to the 4-byte struct alignment = 388.
// SOURCE: EngineSoundLib/DxSound/CharacterSound.h (== [Lib]__EngineSound/.../CharacterSound.h).
constexpr uint32_t kChaSoundDataSize = 388;
// sizeof(EFFANI_PROPERTY_SINGLE) (DxEffAniSingle.h): 4+4+4+4 + sizeof(D3DXMATRIX)(64)
// + 3 * char[128] = 464 for the current property (version 0x0101); the 0x0100 property
// (EFFANI_PROPERTY_SINGLE_100) has no m_matBase: 400.
constexpr uint32_t kEffSinglePropSize101 = 464;
constexpr uint32_t kEffSinglePropSize100 = 400;
// sizeof(EFFANI_PROPERTY_GHOSTING) (DxEffAniGhosting.h): 7 * DWORD = 28.
constexpr uint32_t kEffGhostingPropSize = 28;
// sizeof(EFFANI_PROPERTY_FACEOFF) (DxEffAniFaceOff.h): TCHAR m_szTexture[32] = 32.
constexpr uint32_t kEffFaceOffPropSize = 32;

bool ReadPositionKeys(ep7byte::Stream& s, std::vector<EP7PositionKey>& out) {
    uint32_t count = 0;
    if (!s.ReadU32(count)) return false;
    if (count > kMaxKeysPerTrack) {
        s.Fail("animation position key count exceeds sanity limit");
        return false;
    }
    if (!s.CanReadElements(count, EP7AnimationReader::kPositionKeySize)) {
        s.Fail("animation position key block runs past end of buffer");
        return false;
    }
    out.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        if (!s.ReadU32(out[i].time)) return false;
        for (int k = 0; k < 3; ++k) {
            if (!s.ReadF32(out[i].p[k])) return false;
        }
    }
    return true;
}

bool ReadRotateKeys(ep7byte::Stream& s, std::vector<EP7RotateKey>& out) {
    uint32_t count = 0;
    if (!s.ReadU32(count)) return false;
    if (count > kMaxKeysPerTrack) {
        s.Fail("animation rotate key count exceeds sanity limit");
        return false;
    }
    if (!s.CanReadElements(count, EP7AnimationReader::kRotateKeySize)) {
        s.Fail("animation rotate key block runs past end of buffer");
        return false;
    }
    out.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        if (!s.ReadU32(out[i].time)) return false;
        for (int k = 0; k < 4; ++k) {
            if (!s.ReadF32(out[i].q[k])) return false;
        }
    }
    return true;
}

bool ReadScaleKeys(ep7byte::Stream& s, std::vector<EP7ScaleKey>& out) {
    uint32_t count = 0;
    if (!s.ReadU32(count)) return false;
    if (count > kMaxKeysPerTrack) {
        s.Fail("animation scale key count exceeds sanity limit");
        return false;
    }
    if (!s.CanReadElements(count, EP7AnimationReader::kScaleKeySize)) {
        s.Fail("animation scale key block runs past end of buffer");
        return false;
    }
    out.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        if (!s.ReadU32(out[i].time)) return false;
        for (int k = 0; k < 3; ++k) {
            if (!s.ReadF32(out[i].s[k])) return false;
        }
    }
    return true;
}

bool ReadQuatPosKeys(ep7byte::Stream& s, std::vector<EP7QuatPosKey>& out) {
    uint32_t count = 0;
    if (!s.ReadU32(count)) return false;
    if (count > kMaxKeysPerTrack) {
        s.Fail("animation quatpos key count exceeds sanity limit");
        return false;
    }
    if (!s.CanReadElements(count, EP7AnimationReader::kQuatPosKeySize)) {
        s.Fail("animation quatpos key block runs past end of buffer");
        return false;
    }
    out.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        EP7QuatPosKey& key = out[i];
        if (!s.ReadU32(key.time)) return false;
        for (int k = 0; k < 3; ++k) {
            if (!s.ReadF32(key.scale[k])) return false;
        }
        for (int k = 0; k < 3; ++k) {
            if (!s.ReadF32(key.pos[k])) return false;
        }
        if (!s.ReadU32(key.quatComp0) || !s.ReadU32(key.quatComp1)) return false;
    }
    return true;
}

// DxAniKeys.h:70-88 + LegacyDX_Include/d3dx9math.h:411,416:
//   struct SMatrixKey { DWORD dwTime; D3DXMATRIXA16 mat; };
// D3DXMATRIXA16 carries __declspec(align(16)) on 32-bit MSVC, placing 12 bytes of
// uninitialized struct padding at [4..15] before the 64-byte 4x4 float matrix at
// [16..79] (sizeof(SMatrixKey) == 80). SAnimation::Load_0100, Load_0101, and Load
// (0x0102) all serialize raw sizeof(SMatrixKey) * m_cMatrixKeys bytes via ReadBuffer.
bool ReadMatrixKeys(ep7byte::Stream& s, std::vector<EP7MatrixKey>& out) {
    uint32_t count = 0;
    if (!s.ReadU32(count)) return false;
    if (count > kMaxKeysPerTrack) {
        s.Fail("animation matrix key count exceeds sanity limit");
        return false;
    }
    if (!s.CanReadElements(count, EP7AnimationReader::kMatrixKeySize)) {
        s.Fail("animation matrix key block runs past end of buffer");
        return false;
    }
    out.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        if (!s.ReadU32(out[i].time)) return false;
        if (!s.Skip(EP7AnimationReader::kMatrixKeyAlignPadSize)) return false;
        for (int k = 0; k < 16; ++k) {
            if (!s.ReadF32(out[i].mat[k])) return false;
        }
    }
    return true;
}

// Reads a SAnimation record.  Returns false on malformed input.
// Every SAnimation begins with its own DWORD version (SAnimation::SaveToFile writes
// `SFile << VERSION` first; SAnimation::LoadFromFile reads dwVER and dispatches), so the
// version is read per track and is independent of the container header version.
bool ReadBoneTrack(ep7byte::Stream& s, EP7BoneTrack& out) {
    out = EP7BoneTrack{};

    uint32_t version = 0;
    if (!s.ReadU32(version)) return false;

    if (version == 0x0103) {
        // SAnimation::Load_0103 — verified field order (Lib_Engine/Meshs/SAnimationSaveLoad.cpp:192-248,
        // SaveToFile :250-293):
        //   m_strBone, rotateKeys, dwGarbage1 (= m_strBone.length()), positionKeys,
        //   quatPosKeys, matrixKeys (80B), dwGarbage2 (= m_strBone.length() + 0x0103), scaleKeys.
        if (!s.ReadString(out.boneName, kMaxNameLen)) return false;
        if (!ReadRotateKeys(s, out.rotateKeys)) return false;
        uint32_t garbage = 0;
        if (!s.ReadU32(garbage)) return false;  // written as m_strBone.length()
        if (!ReadPositionKeys(s, out.positionKeys)) return false;
        if (!ReadQuatPosKeys(s, out.quatPosKeys)) return false;
        if (!ReadMatrixKeys(s, out.matrixKeys)) return false;
        if (!s.ReadU32(garbage)) return false;  // written as m_strBone.length() + 0x0103
        if (!ReadScaleKeys(s, out.scaleKeys)) return false;
        return true;
    }

    if (version == 0x0102) {
        // SAnimation::Load (0x0102) — verified field order (enginelib/Meshs/SAnimation.cpp:1515,
        // Lib_Engine/Meshs/SAnimationSaveLoad.cpp:102):
        //   m_strBone, rotateKeys, positionKeys, dwGarbage1, quatPosKeys, dwGarbage2,
        //   matrixKeys, scaleKeys.
        if (!s.ReadString(out.boneName, kMaxNameLen)) return false;
        if (!ReadRotateKeys(s, out.rotateKeys)) return false;
        if (!ReadPositionKeys(s, out.positionKeys)) return false;
        uint32_t garbage = 0;
        if (!s.ReadU32(garbage)) return false;  // written as m_strBone.length()
        if (!ReadQuatPosKeys(s, out.quatPosKeys)) return false;
        if (!s.ReadU32(garbage)) return false;  // written as m_strBone.length() + VERSION
        if (!ReadMatrixKeys(s, out.matrixKeys)) return false;
        if (!ReadScaleKeys(s, out.scaleKeys)) return false;
        return true;
    }

    if (version == 0x0101) {
        // SAnimation::Load_0101 — verified field order (enginelib/Meshs/SAnimation.cpp:1464,
        // Lib_Engine/Meshs/SAnimationSaveLoad.cpp:52):
        //   positionKeys, rotateKeys, scaleKeys, matrixKeys, quatPosKeys, m_strBone.
        if (!ReadPositionKeys(s, out.positionKeys)) return false;
        if (!ReadRotateKeys(s, out.rotateKeys)) return false;
        if (!ReadScaleKeys(s, out.scaleKeys)) return false;
        if (!ReadMatrixKeys(s, out.matrixKeys)) return false;
        if (!ReadQuatPosKeys(s, out.quatPosKeys)) return false;
        if (!s.ReadString(out.boneName, kMaxNameLen)) return false;
        return true;
    }

    if (version == 0x0100) {
        // SAnimation::Load_0100 — verified field order (enginelib/Meshs/SAnimation.cpp:1419,
        // Lib_Engine/Meshs/SAnimationSaveLoad.cpp:15):
        //   positionKeys, rotateKeys, scaleKeys, matrixKeys, m_strBone (no quatPosKeys).
        // Empty bone names (m_strBone == "") occur in authentic 0x0100 assets and are
        // valid in SAnimation::Load_0100 (FindBone("") -> NULL / boneIndex == -1).
        if (!ReadPositionKeys(s, out.positionKeys)) return false;
        if (!ReadRotateKeys(s, out.rotateKeys)) return false;
        if (!ReadScaleKeys(s, out.scaleKeys)) return false;
        if (!ReadMatrixKeys(s, out.matrixKeys)) return false;
        if (!s.ReadString(out.boneName, kMaxNameLen)) return false;
        return true;
    }

    char message[112];
    std::snprintf(message, sizeof(message),
                  "unsupported animation track version 0x%08X (implemented: 0x0100, 0x0101, 0x0102, 0x0103)",
                  version);
    s.Fail(message);
    return false;
}

bool ReadTrackList(ep7byte::Stream& s, std::vector<EP7BoneTrack>& out, bool append = false) {
    uint32_t count = 0;
    if (!s.ReadU32(count)) return false;
    if (!append) {
        out.clear();
    }
    if (count > kMaxBoneTracks || count > kMaxBoneTracks - out.size()) {
        s.Fail("animation bone track count exceeds sanity limit");
        return false;
    }
    out.reserve(out.size() + count);
    for (uint32_t i = 0; i < count; ++i) {
        EP7BoneTrack track;
        if (!ReadBoneTrack(s, track)) return false;
        out.push_back(std::move(track));
    }
    return true;
}

inline bool IsFinite(float v) { return std::isfinite(v); }

// ---------------------------------------------------------------------------
// B16: SANIMCONINFO 0x0108..0x0115 sub-parsers.
//
// SOURCE: backup repo matheomedrana/Ran-Online-EP7-Complete-Files @ tf661fbf,
// "(Source Code) Ran Online Server Files and Developer Tools/Lib_Engine/Meshs/
// SAnimationInfoSaveLoad.cpp" (LFS SHA-256 80de6adeb3bd1ec2892740bbdb93dc10209d6a38f0b5b73bb4ad923187703837)
// and the headers it includes (SAnimationInfo.h, DxAniBoneScale.cpp/h,
// ../DxEffect/EffAni/DxEffAni*.h/cpp, EngineSoundLib/DxSound/CharacterSound.h).
// The GDrive EP7 drop ([Lib]__Engine) agrees field-for-field on 0x0101..0x0107 and on
// every effect/AniScale layout; it simply predates the 0x0108+ loaders.
// ---------------------------------------------------------------------------

// Reads one fixed-size char field (m_szEff[128], m_szTexture[32], ...) and stops at the
// first NUL the way StringCchCopyA consumes the buffer the engine copies it into.
bool ReadFixedCharField(ep7byte::Stream& s, size_t fieldBytes, std::string& out) {
    if (fieldBytes > s.Remaining()) {
        s.Fail("truncated cfg fixed char field");
        return false;
    }
    std::vector<uint8_t> buf(fieldBytes);
    if (!s.ReadRaw(buf.data(), fieldBytes)) return false;
    size_t n = 0;
    while (n < fieldBytes && buf[n] != 0) ++n;
    out.assign(reinterpret_cast<const char*>(buf.data()), n);
    return true;
}

// m_wStrikeCount + SANIMSTRIKE m_sStrikeEff[ACF_STRIKE=9].
// sizeof(SANIMSTRIKE) = EMPIECECHAR(4) + EMSTRIKE_EFF(4) + DWORD(4) = 12.
// SOURCE: SAnimationInfo.h (SANIMSTRIKE, EMANICONINFO::ACF_STRIKE=9), DxPieceDefine.h.
bool ParseCfgStrikeTable(ep7byte::Stream& s, EP7AnimInfo& info) {
    if (!s.ReadU16(info.strikeCount)) {
        s.Fail("truncated cfg strike count");
        return false;
    }
    if (!s.CanReadElements(EP7AnimInfo::kStrike, 12)) {
        s.Fail("truncated cfg strike table");
        return false;
    }
    for (uint32_t i = 0; i < EP7AnimInfo::kStrike; ++i) {
        uint32_t piece = 0;
        uint32_t effect = 0;
        uint32_t frame = 0;
        if (!s.ReadU32(piece) || !s.ReadU32(effect) || !s.ReadU32(frame)) {
            s.Fail("truncated cfg strike entry");
            return false;
        }
        info.strikeEff[i].piece = static_cast<int32_t>(piece);
        info.strikeEff[i].effect = static_cast<int32_t>(effect);
        info.strikeEff[i].frame = frame;
    }
    return true;
}

// m_wDivCount + WORD m_wDivFrame[ACF_DIV=8].
bool ParseCfgDivFrames(ep7byte::Stream& s, EP7AnimInfo& info) {
    if (!s.ReadU16(info.divCount)) {
        s.Fail("truncated cfg div count");
        return false;
    }
    for (uint32_t i = 0; i < EP7AnimInfo::kDiv; ++i) {
        if (!s.ReadU16(info.divFrame[i])) {
            s.Fail("truncated cfg div frames");
            return false;
        }
    }
    return true;
}

// DWORD m_MainType + DWORD m_SubType (written as DWORDs in every 0x0105+ layout).
bool ParseCfgTypes(ep7byte::Stream& s, EP7AnimInfo& info) {
    uint32_t mainType = 0;
    uint32_t subType = 0;
    if (!s.ReadU32(mainType) || !s.ReadU32(subType)) {
        s.Fail("truncated cfg animation type");
        return false;
    }
    // AN_NONE (34) is the "no type" sentinel, never a valid file value; the authentic
    // GetAnimation lookups index [0, AN_TYPE_SIZE) = [0, 34).
    if (mainType >= static_cast<uint32_t>(EP7AnimMainType::None)) {
        s.Fail("cfg main type out of range");
        return false;
    }
    info.mainType = static_cast<int32_t>(mainType);
    info.subType = static_cast<int32_t>(subType);
    return true;
}

// One m_listEffAni entry: DWORD dwType, then DxEffAniData_<T>::LoadFile, which reads
// DWORD dwTypeID, DWORD dwVer, DWORD dwSize, then the property.
//
// Per-type layouts (MSVC x86, default alignment):
//   Single  (EMEFFANI_SINGLE=0), property 0x0101: DWORD flags, DWORD frame, float fact,
//           float scale, D3DXMATRIX matBase(64), char szEff[128], char szBipFirst[128],
//           char szBipSecond[128]  -> 464 bytes.
//           property 0x0100 (EFFANI_PROPERTY_SINGLE_100): same without matBase -> 400.
//           Any other version returns E_FAIL WITHOUT consuming dwSize bytes, which
//           desynchronises the stream for everything after it -- reject.
//   Ghosting(EMEFFANI_GHOSTING=1), property 0x0100: 7 * DWORD -> 28 bytes. Other
//           versions skip dwSize bytes (authentic SetOffSet) and continue.
//   Trace   (EMEFFANI_TRACE=2), property 0x0100: float + std::string member, so the
//           property size is build-dependent; the declared dwSize is authoritative and
//           the property blob is consumed, not decoded. Other versions skip dwSize.
//   FaceOff (EMEFFANI_FACEOFF=3), property 0x0100: char m_szTexture[32] -> 32 bytes.
//           Other versions skip dwSize.
//
// SOURCE: DxEffAniSingle.cpp / DxEffAniGhosting.cpp / DxEffAniTrace.cpp /
// DxEffAniFaceOff.cpp (LoadFile + SaveFile + TYPEID/VERSION statics); identical in the
// local enginelib tree and the EP7 GDrive drop.
bool ParseCfgEffectEntry(ep7byte::Stream& s, EP7AnimInfo::CfgEffectAni& out) {
    uint32_t outerType = 0;
    if (!s.ReadU32(outerType)) {
        s.Fail("truncated cfg effect type");
        return false;
    }
    // DxEffAniMan::CreateEffInstance (DxEffAni.cpp) knows exactly EMEFFANI_SINGLE=0,
    // EMEFFANI_GHOSTING=1, EMEFFANI_TRACE=2, EMEFFANI_FACEOFF=3 and returns NULL for
    // anything else. The authentic loop then reads nothing for that entry, which
    // desynchronises the stream; a mobile parser cannot stay aligned there, so reject.
    if (outerType > 3u) {
        s.Fail("cfg effect type unknown (EMEFFANI_SINGLE..FACEOFF = 0..3)");
        return false;
    }
    out.typeId = outerType;

    uint32_t innerType = 0;
    if (!s.ReadU32(innerType)) {
        s.Fail("truncated cfg effect inner type");
        return false;
    }
    // The writer emits the type id twice: once in the SANIMCONINFO loop and once inside
    // DxEffAniData::SaveFile. A mismatch means the stream is not where the authentic
    // reader expects it.
    if (innerType != outerType) {
        s.Fail("cfg effect inner type does not match its list type");
        return false;
    }
    if (!s.ReadU32(out.effectVersion) || !s.ReadU32(out.sizeBytes)) {
        s.Fail("truncated cfg effect property header");
        return false;
    }

    switch (outerType) {
        case 0: {  // DxEffAniData_Single
            if (out.effectVersion == 0x0101u) {
                if (out.sizeBytes != kEffSinglePropSize101) {
                    s.Fail("cfg Single effect property size mismatch (0x0101)");
                    return false;
                }
            } else if (out.effectVersion == 0x0100u) {
                if (out.sizeBytes != kEffSinglePropSize100) {
                    s.Fail("cfg Single effect property size mismatch (0x0100)");
                    return false;
                }
            } else {
                s.Fail("cfg Single effect version unsupported (authentic loader fails without skipping)");
                return false;
            }
            if (!s.ReadU32(out.flags) || !s.ReadU32(out.frame) ||
                !s.ReadF32(out.fact) || !s.ReadF32(out.scale)) {
                s.Fail("truncated cfg Single effect property");
                return false;
            }
            if (out.effectVersion == 0x0101u && !s.Skip(64)) {  // D3DXMATRIX m_matBase
                s.Fail("truncated cfg Single effect matrix");
                return false;
            }
            std::string eff;
            std::string bipFirst;
            std::string bipSecond;
            if (!ReadFixedCharField(s, 128, eff) ||
                !ReadFixedCharField(s, 128, bipFirst) ||
                !ReadFixedCharField(s, 128, bipSecond)) {
                s.Fail("truncated cfg Single effect names");
                return false;
            }
            out.name = eff;  // m_szEff; m_szBipFirst / m_szBipSecond consumed above
            break;
        }
        case 1: {  // DxEffAniData_Ghosting
            if (out.effectVersion != 0x0100u) {
                // Authentic behaviour: skip the declared property block and continue.
                if (out.sizeBytes == 0 || out.sizeBytes > s.Remaining()) {
                    s.Fail("cfg Ghosting effect property size runs past end of buffer");
                    return false;
                }
                if (!s.Skip(out.sizeBytes)) return false;
                break;
            }
            if (out.sizeBytes != kEffGhostingPropSize) {
                s.Fail("cfg Ghosting effect property size mismatch");
                return false;
            }
            if (!s.ReadU32(out.flags) || !s.ReadU32(out.frame) || !s.ReadU32(out.frame2)) {
                s.Fail("truncated cfg Ghosting effect property");
                return false;
            }
            // m_dwGhostingNum, m_dwAlphaMax, m_dwUnit, D3DCOLOR m_dwColor: consumed.
            if (!s.Skip(16)) {
                s.Fail("truncated cfg Ghosting effect property");
                return false;
            }
            break;
        }
        case 2: {  // DxEffAniData_Trace
            if (out.effectVersion != 0x0100u) {
                if (out.sizeBytes == 0 || out.sizeBytes > s.Remaining()) {
                    s.Fail("cfg Trace effect property size runs past end of buffer");
                    return false;
                }
                if (!s.Skip(out.sizeBytes)) return false;
                break;
            }
            // float m_fScale must at least fit; the rest is the std::string member blob
            // whose layout belongs to the client build that wrote the file.
            if (out.sizeBytes < 4 || out.sizeBytes > s.Remaining()) {
                s.Fail("cfg Trace effect property size invalid");
                return false;
            }
            if (!s.Skip(out.sizeBytes)) return false;
            break;
        }
        case 3: {  // DxEffAniData_FaceOff
            if (out.effectVersion != 0x0100u) {
                if (out.sizeBytes == 0 || out.sizeBytes > s.Remaining()) {
                    s.Fail("cfg FaceOff effect property size runs past end of buffer");
                    return false;
                }
                if (!s.Skip(out.sizeBytes)) return false;
                break;
            }
            if (out.sizeBytes != kEffFaceOffPropSize) {
                s.Fail("cfg FaceOff effect property size mismatch");
                return false;
            }
            std::string texture;
            if (!ReadFixedCharField(s, 32, texture)) {
                s.Fail("truncated cfg FaceOff effect texture");
                return false;
            }
            out.name = texture;
            break;
        }
        default:
            s.Fail("cfg effect type unknown (EMEFFANI_SINGLE..FACEOFF = 0..3)");
            return false;
    }
    return true;
}

// SANIMCONINFO::LoadFile_0108_0109_0110 / LoadFile_0111_0112_0114_0115 m_listEffAni loop:
// DWORD dwSize, then dwSize * (DWORD dwType + DxEffAniData::LoadFile).
bool ParseCfgEffects(ep7byte::Stream& s, EP7AnimInfo& info) {
    uint32_t effectCount = 0;
    if (!s.ReadU32(effectCount)) {
        s.Fail("truncated cfg effect count");
        return false;
    }
    info.effectCount = effectCount;
    if (effectCount > kMaxCfgEffects) {
        s.Fail("cfg effect count exceeds sanity limit");
        return false;
    }
    // Minimum entry: outer type + inner type + version + size = 16 bytes.
    if (!s.CanReadElements(effectCount, 16)) {
        s.Fail("cfg effect list runs past end of buffer");
        return false;
    }
    info.effects.reserve(effectCount);
    for (uint32_t i = 0; i < effectCount; ++i) {
        EP7AnimInfo::CfgEffectAni entry;
        if (!ParseCfgEffectEntry(s, entry)) return false;
        info.effects.push_back(std::move(entry));
    }
    info.effectsSkipped = false;
    return true;
}

// DxAniScale::Load (DxAniBoneScale.cpp): DWORD mapSize, then per entry a length-prefixed
// TSTRING anim name plus a DxAniScaleMAIN block (DWORD count + count * (DWORD key +
// float scale)). Scale key values are consumed for stream alignment and bounds-checked,
// but not retained; the entry names and key counts are recorded on EP7AnimInfo.
bool ParseCfgAniScale(ep7byte::Stream& s, EP7AnimInfo& info) {
    uint32_t mapSize = 0;
    if (!s.ReadU32(mapSize)) {
        s.Fail("truncated cfg ani scale map size");
        return false;
    }
    if (mapSize > kMaxAniScaleEntries) {
        s.Fail("cfg ani scale entry count exceeds sanity limit");
        return false;
    }
    // Minimum entry: 4-byte name length + 1 byte of name + 4-byte key count = 9 bytes.
    if (!s.CanReadElements(mapSize, 9)) {
        s.Fail("cfg ani scale map runs past end of buffer");
        return false;
    }
    info.aniScaleEntries.reserve(mapSize);
    for (uint32_t i = 0; i < mapSize; ++i) {
        EP7AnimInfo::CfgAniScaleEntry entry;
        if (!s.ReadString(entry.animName, kMaxNameLen)) {
            s.Fail("truncated cfg ani scale anim name");
            return false;
        }
        uint32_t keyCount = 0;
        if (!s.ReadU32(keyCount)) {
            s.Fail("truncated cfg ani scale key count");
            return false;
        }
        // Each key: DWORD dwKey + float fScale = 8 bytes.
        if (!s.CanReadElements(keyCount, 8)) {
            s.Fail("cfg ani scale keys run past end of buffer");
            return false;
        }
        for (uint32_t k = 0; k < keyCount; ++k) {
            uint32_t keyTime = 0;
            float scale = 0.0f;
            if (!s.ReadU32(keyTime) || !s.ReadF32(scale)) {
                s.Fail("truncated cfg ani scale key");
                return false;
            }
        }
        entry.keyCount = keyCount;
        info.aniScaleEntries.push_back(std::move(entry));
    }
    return true;
}

}  // namespace

// ---------------------------------------------------------------------------
// EP7AnimClip
// ---------------------------------------------------------------------------

uint32_t EP7AnimClip::MaxKeyTime() const {
    uint32_t best = 0;
    auto scan = [&best](const std::vector<EP7BoneTrack>& tracks) {
        for (const auto& track : tracks) {
            if (!track.matrixKeys.empty()) {
                const uint32_t t = track.matrixKeys.back().time;
                if (t > best) best = t;
            }
            if (!track.quatPosKeys.empty()) {
                const uint32_t t = track.quatPosKeys.back().time;
                if (t > best) best = t;
            }
            if (!track.rotateKeys.empty()) {
                const uint32_t t = track.rotateKeys.back().time;
                if (t > best) best = t;
            }
            if (!track.positionKeys.empty()) {
                const uint32_t t = track.positionKeys.back().time;
                if (t > best) best = t;
            }
            if (!track.scaleKeys.empty()) {
                const uint32_t t = track.scaleKeys.back().time;
                if (t > best) best = t;
            }
        }
    };
    scan(mainTracks);
    scan(upBodyTracks);
    return best;
}

// ---------------------------------------------------------------------------
// EP7AnimationReader
// ---------------------------------------------------------------------------

EP7AnimClip EP7AnimationReader::ParseContainer(const std::vector<uint8_t>& bytes, bool hasHeader, bool compByteEncoded) {
    EP7AnimClip clip;

    ep7byte::Stream stream;
    stream.Attach(bytes, compByteEncoded);

    if (hasHeader) {
        std::string fileType;
        uint32_t fileId = 0;
        if (!stream.ReadFileType(fileType, fileId)) {
            clip.error = "animation container header unreadable";
            return clip;
        }
    }

    uint32_t version = 0;
    if (hasHeader) {
        // SAnimContainer::SaveToFile calls SetFileType(szFILETYPE, VERSION), so the
        // container version lives in the 132-byte basestream header, and the payload
        // starts directly with the first list count.
        std::string fileType;
        uint32_t fileId = 0;
        if (!stream.Seek(0)) {
            clip.error = "animation container seek failed";
            return clip;
        }
        // Re-derive the header values without re-attaching.
        ep7byte::Stream probe;
        probe.Attach(std::vector<uint8_t>(bytes.begin(), bytes.begin() + static_cast<long>(ep7byte::Stream::kHeaderSize)), compByteEncoded);
        if (!probe.ReadFileType(fileType, fileId)) {
            clip.error = "animation container header unreadable";
            return clip;
        }
        version = fileId;
    } else {
        if (!stream.ReadU32(version)) {
            clip.error = "truncated animation container version";
            return clip;
        }
    }
    clip.version = version;

    if (version == 0x0104) {
        // SAnimContainer::Load_0104 (Lib_Engine/Meshs/SAnimationSaveLoad.cpp:444-479, 670-699):
        // main list (m_listAnimation), then two consecutive up-body lists both appended in
        // order to m_listAniUPBODY.
        if (!ReadTrackList(stream, clip.mainTracks)) {
            clip.error = stream.Error().empty() ? "invalid main track list" : stream.Error();
            return clip;
        }
        if (!ReadTrackList(stream, clip.upBodyTracks)) {
            clip.error = stream.Error().empty() ? "invalid up-body track list" : stream.Error();
            return clip;
        }
        if (!ReadTrackList(stream, clip.upBodyTracks, /*append=*/true)) {
            clip.error = stream.Error().empty() ? "invalid secondary up-body track list"
                                                : stream.Error();
            return clip;
        }
    } else if (version == 0x0103) {
        // SAnimContainer::Load_0103 (Lib_Engine/Meshs/SAnimationSaveLoad.cpp:412-442):
        // up-body list, DWORD dwGarbageValue (= 0x0103 + numUpBody + numMain), then main list.
        if (!ReadTrackList(stream, clip.upBodyTracks)) {
            clip.error = stream.Error().empty() ? "invalid up-body track list" : stream.Error();
            return clip;
        }
        uint32_t garbageValue = 0;
        if (!stream.ReadU32(garbageValue)) {
            clip.error = stream.Error().empty() ? "truncated 0x0103 animation container inter-list word"
                                                : stream.Error();
            return clip;
        }
        if (!ReadTrackList(stream, clip.mainTracks)) {
            clip.error = stream.Error().empty() ? "invalid main track list" : stream.Error();
            return clip;
        }
    } else if (version == 0x0102) {
        // SAnimContainer::Load: up-body list first, then the main list.
        if (!ReadTrackList(stream, clip.upBodyTracks)) {
            clip.error = stream.Error().empty() ? "invalid up-body track list" : stream.Error();
            return clip;
        }
        if (!ReadTrackList(stream, clip.mainTracks)) {
            clip.error = stream.Error().empty() ? "invalid main track list" : stream.Error();
            return clip;
        }
    } else if (version == 0x0101) {
        // SAnimContainer::Load_0101: main list, then up-body list (single counts each).
        if (!ReadTrackList(stream, clip.mainTracks)) {
            clip.error = stream.Error().empty() ? "invalid main track list" : stream.Error();
            return clip;
        }
        if (!ReadTrackList(stream, clip.upBodyTracks)) {
            clip.error = stream.Error().empty() ? "invalid up-body track list" : stream.Error();
            return clip;
        }
    } else if (version == 0x0100) {
        // SAnimContainer::Load_0100: a single main list only.
        if (!ReadTrackList(stream, clip.mainTracks)) {
            clip.error = stream.Error().empty() ? "invalid main track list" : stream.Error();
            return clip;
        }
    } else {
        // Report the actual value: the authentic client contains containers this build does
        // not implement, and a bare "unsupported version" hides which ones are missing.
        char message[160];
        std::snprintf(message, sizeof(message),
                      "unsupported animation container version 0x%08X (implemented: 0x0100, 0x0101, 0x0102, 0x0103, 0x0104)",
                      version);
        clip.error = message;
        return clip;
    }

    clip.valid = true;
    return clip;
}

// The engine writes SetFileType("SANIMCONINFO", VERSION). A packer may write a shorter or
// empty type string, so an empty type is treated as "unknown, assume same family"; a
// non-empty, different type is a genuine mismatch.
static bool IsSanimconinfoType(const std::string& fileType) {
    if (fileType.empty()) return true;  // unknown; do not accuse the file
    return fileType.rfind("SANIMCONINFO", 0) == 0;
}

EP7AnimInfo EP7AnimationReader::ParseInfo(const std::vector<uint8_t>& bytes, bool hasHeader, bool compByteEncoded) {
    EP7AnimInfo info;

    ep7byte::Stream stream;
    stream.Attach(bytes, compByteEncoded);

    if (hasHeader) {
        std::string fileType;
        uint32_t fileId = 0;
        if (!stream.ReadFileType(fileType, fileId)) {
            info.error = "animation cfg header unreadable";
            return info;
        }
        info.fileType = fileType;
    } else {
        info.error = "SANIMCONINFO requires the basestream header (version lives in it)";
        return info;
    }

    uint32_t version = 0;
    if (hasHeader) {
        // SANIMCONINFO::SaveFile uses SetFileType("SANIMCONINFO", VERSION) and then writes
        // m_szName immediately, so the payload has no leading version DWORD.
        ep7byte::Stream probe;
        probe.Attach(std::vector<uint8_t>(bytes.begin(), bytes.begin() + static_cast<long>(ep7byte::Stream::kHeaderSize)), compByteEncoded);
        std::string fileType;
        uint32_t fileId = 0;
        if (!probe.ReadFileType(fileType, fileId)) {
            info.error = "animation cfg header unreadable";
            return info;
        }
        version = fileId;
    } else {
        if (!stream.ReadU32(version)) {
            info.error = "truncated animation cfg version";
            return info;
        }
    }

    // Supported layouts, all taken from authentic engine sources:
    //
    // 0x0101..0x0107 -- enginelib/Meshs/DxSkinAniManSaveLoad.cpp (older engine tree,
    // identical field-for-field in the EP7 backup and GDrive trees):
    //
    //   0x0101  SANIMCONINFO_101  no strike table, no sound block
    //   0x0102  SANIMCONINFO_102  + SChaSoundData_102 (6 slots)
    //   0x0103  SANIMCONINFO_103  + SChaSoundData_103 (10 slots, LOOP+TYPE+FRAME)
    //   0x0104  SANIMCONINFO_104  + strike table
    //   0x0105  SANIMCONINFO_105  same layout, written field-by-field
    //   0x0106  SANIMCONINFO_106  + SChaSoundData (no TYPE)
    //   0x0107  SANIMCONINFO      + trailing DxAniScale block
    //
    // The common prefix (name, skeleton, flags, time window, type, div frames) is identical
    // for all of them, so parsing proceeds member by member and stops at the effect count.
    // On-disk size of the SANIMCONINFO header part, per version.
    //
    // These are MEASURED from the shipped client and then explained by the struct layout;
    // they are not guesses. The key detail: the prefix ends at offset 302 on a WORD, and
    // that WORD is m_wStrikeCount -- so it occupies what would otherwise be alignment
    // padding, and SANIMSTRIKE m_sStrikeEff[9] starts 4-aligned at 304 with no gap:
    //
    //   302  prefix (name 128, skeleton 128, flag + 4 times 20, 2 types 8, count 2,
    //        div frames 16)  -- no tail padding, the strike count fills it
    //   +2   m_wStrikeCount (versions >= 0x0104)
    //   +108 SANIMSTRIKE[9]
    //   +428 SChaSoundData_103  (0x0102: +260 SChaSoundData_102, 0x0106/7: +388)
    //
    //   0x0101 302   0x0102 564   0x0103 732   0x0104 840
    //   0x0105 840   0x0106 800   0x0107 800
    //
    // CONFIRMED against authentic data: 0x0104 files carry exactly 538 bytes after the
    // 302-byte prefix (110 + 428) and 0x0106/0x0107 files exactly 498 (110 + 388).
    //
    // 0x0108..0x0115 -- Lib_Engine/Meshs/SAnimationInfoSaveLoad.cpp (backup repo
    // matheomedrana/Ran-Online-EP7-Complete-Files @ tf661fbf): LoadFile_0108_0109_0110 and
    // LoadFile_0111_0112_0114_0115, both marked "//official version partial read only" in
    // SAnimationInfo.h. They are parsed field-by-field below; 0x0113 has no case in the
    // authentic LoadFile switch and stays unsupported.
    //
    // SOURCE: enginelib/Meshs/DxSkinAniManSaveLoad.cpp, enginelib/Meshs/SAnimation.h,
    //         EngineSoundLib/DxSound/CharacterSound.h (0x0101..0x0107); backup repo
    //         Lib_Engine/Meshs/SAnimationInfo{,.h,SaveLoad.cpp}, DxAniKeys.h,
    //         DxAniBoneScale.cpp, DxEffect/EffAni/DxEffAni*.cpp (0x0108..0x0115).
    static constexpr uint32_t kInfoSize0101 = 302;
    static constexpr uint32_t kInfoSize0102 = 564;
    static constexpr uint32_t kInfoSize0103 = 732;
    static constexpr uint32_t kInfoSize0104 = 840;
    static constexpr uint32_t kInfoSize0105 = 840;
    static constexpr uint32_t kInfoSize0106 = 800;
    static constexpr uint32_t kInfoSize0107 = 800;

    if (version < 0x0101 || version > EP7AnimInfo::kVersionCfgLast || version == 0x0113) {
        // 0x0113 is absent from the authentic LoadFile switch (0x0115, 0x0114, 0x0112,
        // 0x0111, 0x0110, 0x0109, 0x0108, 0x0107, ... and reaches ErrorVersion);
        // everything below 0x0101 or above 0x0115 has no authentic definition either.
        // Report those as UNSUPPORTED -- a known, tracked coverage gap (BLOCKERS.md
        // B11/B16), not a defect in this reader. `unsupported` is claimed only for
        // files whose basestream type really is SANIMCONINFO; a different type is a
        // genuine mismatch and must not be excused.
        char message[240];
        if (IsSanimconinfoType(info.fileType)) {
            info.unsupported = true;
            std::snprintf(message, sizeof(message),
                          "UNSUPPORTED animation cfg version 0x%08X (basestream type \"%s\"; this "
                          "build implements 0x0101-0x0112 and 0x0114-0x0115 -- see B11/B16)",
                          version, info.fileType.c_str());
        } else {
            std::snprintf(message, sizeof(message),
                          "not a SANIMCONINFO file: basestream type is \"%s\" but layout 0x%08X was "
                          "expected (implemented 0x0101-0x0115 except 0x0113)",
                          info.fileType.c_str(), version);
        }
        info.error = message;
        return info;
    }
    info.version = version;

    auto readFixedName = [&stream](std::string& out) -> bool {
        if (stream.Remaining() < EP7AnimInfo::kNameSize) {
            stream.Fail("truncated fixed name field");
            return false;
        }
        std::vector<uint8_t> buf(EP7AnimInfo::kNameSize);
        if (!stream.ReadRaw(buf.data(), EP7AnimInfo::kNameSize)) return false;
        size_t n = buf.size();
        while (n > 0 && buf[n - 1] == 0) --n;
        out.assign(reinterpret_cast<const char*>(buf.data()), n);
        return true;
    };

    // -----------------------------------------------------------------
    // B16: 0x0108..0x0110 and 0x0111/0x0112/0x0114/0x0115.
    //
    // SOURCE: backup repo matheomedrana/Ran-Online-EP7-Complete-Files @ tf661fbf,
    // Lib_Engine/Meshs/SAnimationInfoSaveLoad.cpp, LoadFile_0108_0109_0110 and
    // LoadFile_0111_0112_0114_0115. Both are marked "//official version partial read
    // only" in SAnimationInfo.h:268-270 -- the official client wrote fields beyond the
    // last one these loaders read. Anything left after the final authentic read is
    // counted in trailingBytes and never reinterpreted; the file stays valid because
    // the authentic LoadFile returns TRUE regardless (only GetFileType failures or an
    // unknown version return FALSE).
    // -----------------------------------------------------------------
    if (version >= 0x0108) {
        if (version <= 0x0110) {
            // LoadFile_0108_0109_0110: header names, then m_dwFlag + m_UNITTIME +
            // m_dwSTime + m_dwETime + m_dwETimeOrig -- NOTE the order: m_UNITTIME is
            // written BEFORE m_dwSTime, unlike 0x0101..0x0107 -- then the strike table
            // BEFORE the div frames, then the effect list, the DxAniScale block and
            // finally the SChaSoundData block.
            if (!readFixedName(info.name)) {
                info.error = "invalid cfg name";
                return info;
            }
            if (!readFixedName(info.skeleton)) {
                info.error = "invalid cfg skeleton name";
                return info;
            }
            if (!stream.ReadU32(info.flag)) {
                info.error = "truncated cfg flag";
                return info;
            }
            if (!stream.ReadU32(info.unitTime) || !stream.ReadU32(info.startTime) ||
                !stream.ReadU32(info.endTime) || !stream.ReadU32(info.endTimeOrig)) {
                info.error = "truncated cfg time window";
                return info;
            }
            if (!ParseCfgTypes(stream, info) || !ParseCfgStrikeTable(stream, info) ||
                !ParseCfgDivFrames(stream, info)) {
                info.error = stream.Error().empty() ? "invalid cfg header tail" : stream.Error();
                return info;
            }
            if (!ParseCfgEffects(stream, info)) {
                info.error = stream.Error().empty() ? "invalid cfg effect list" : stream.Error();
                return info;
            }
            if (!ParseCfgAniScale(stream, info)) {
                info.error = stream.Error().empty() ? "invalid cfg ani scale block" : stream.Error();
                return info;
            }
            // The loader ends with SChaSoundData (388 bytes), consumed verbatim like the
            // 0x0106/0x0107 sound block (this port uses none of the sound fields).
            if (!stream.Skip(kChaSoundDataSize)) {
                info.error = "truncated cfg sound block";
                return info;
            }
        } else {
            // LoadFile_0111_0112_0114_0115: four length-prefixed strings -- szCfgFile
            // (read and discarded), szXFile -> m_szName, szBinFile (read and discarded),
            // szSkeleton -> m_szSkeletion -- then m_dwFlag + m_dwSTime + m_dwETime +
            // m_dwETimeOrig + m_UNITTIME, div frames BEFORE the strike table, the
            // SChaSoundData block, the effect list and the DxAniScale block.
            if (!stream.ReadString(info.cfgFileName, kMaxNameLen) ||
                !stream.ReadString(info.name, kMaxNameLen) ||
                !stream.ReadString(info.binFileName, kMaxNameLen) ||
                !stream.ReadString(info.skeleton, kMaxNameLen)) {
                info.error = "truncated cfg companion-name strings";
                return info;
            }
            if (!stream.ReadU32(info.flag)) {
                info.error = "truncated cfg flag";
                return info;
            }
            if (!stream.ReadU32(info.startTime) || !stream.ReadU32(info.endTime) ||
                !stream.ReadU32(info.endTimeOrig) || !stream.ReadU32(info.unitTime)) {
                info.error = "truncated cfg time window";
                return info;
            }
            if (!ParseCfgTypes(stream, info) || !ParseCfgDivFrames(stream, info) ||
                !ParseCfgStrikeTable(stream, info)) {
                info.error = stream.Error().empty() ? "invalid cfg header tail" : stream.Error();
                return info;
            }
            if (!stream.Skip(kChaSoundDataSize)) {
                info.error = "truncated cfg sound block";
                return info;
            }
            if (!ParseCfgEffects(stream, info)) {
                info.error = stream.Error().empty() ? "invalid cfg effect list" : stream.Error();
                return info;
            }
            if (!ParseCfgAniScale(stream, info)) {
                info.error = stream.Error().empty() ? "invalid cfg ani scale block" : stream.Error();
                return info;
            }
        }

        // End of the authentic read sequence: record what the official writer left
        // behind instead of dropping it silently.
        info.trailingBytes = stream.Remaining();
        info.exactEof = info.trailingBytes == 0;
        info.valid = true;
        return info;
    }

    if (!readFixedName(info.name)) {
        info.error = "invalid cfg name";
        return info;
    }
    if (!readFixedName(info.skeleton)) {
        info.error = "invalid cfg skeleton name";
        return info;
    }

    if (!stream.ReadU32(info.flag)) {
        info.error = "truncated cfg flag";
        return info;
    }
    if (!stream.ReadU32(info.startTime) || !stream.ReadU32(info.endTime) ||
        !stream.ReadU32(info.endTimeOrig) || !stream.ReadU32(info.unitTime)) {
        info.error = "truncated cfg time window";
        return info;
    }

    uint32_t mainType = 0;
    uint32_t subType = 0;
    if (!stream.ReadU32(mainType) || !stream.ReadU32(subType)) {
        info.error = "truncated cfg animation type";
        return info;
    }
    // EMANI_MAINTYPE: AN_NONE is the "no type" sentinel and never a valid file value;
    // B16 widened the valid range 0..20 to 0..33 (EP7 AN_TYPE_SIZE = 34, DxAniKeys.h).
    if (mainType >= static_cast<uint32_t>(EP7AnimMainType::None)) {
        info.error = "cfg main type out of range";
        return info;
    }
    info.mainType = static_cast<int32_t>(mainType);
    info.subType = static_cast<int32_t>(subType);

    if (!stream.ReadU16(info.divCount)) {
        info.error = "truncated cfg div count";
        return info;
    }
    for (uint32_t i = 0; i < EP7AnimInfo::kDiv; ++i) {
        if (!stream.ReadU16(info.divFrame[i])) {
            info.error = "truncated cfg div frames";
            return info;
        }
    }

    // Everything from here to the end of the header is the strike table and the sound
    // block, whose size depends on the version AND on MSVC struct padding. Rather than
    // re-deriving those layouts field by field (and getting the padding wrong), skip to
    // exactly the offset the engine reaches. The engine copies those bytes verbatim and
    // this port uses none of them.
    uint32_t infoSize = 0;
    switch (version) {
        case 0x0101: infoSize = kInfoSize0101; break;
        case 0x0102: infoSize = kInfoSize0102; break;
        case 0x0103: infoSize = kInfoSize0103; break;
        case 0x0104: infoSize = kInfoSize0104; break;
        case 0x0105: infoSize = kInfoSize0105; break;
        case 0x0106: infoSize = kInfoSize0106; break;
        default:     infoSize = kInfoSize0107; break;
    }
    // Prefix consumed so far: 128 + 128 + 4*5 (flag + 4 times) + 4*2 (types) + 2 + 8*2.
    const uint32_t consumed = 128 + 128 + 20 + 8 + 2 + 16;
    if (infoSize < consumed) {
        info.error = "internal: cfg info size smaller than its prefix";
        return info;
    }
    if (stream.Remaining() < infoSize - consumed) {
        char message[160];
        std::snprintf(message, sizeof(message),
                      "truncated cfg: layout 0x%04X needs %u bytes after the prefix, %zu available",
                      version, infoSize - consumed, stream.Remaining());
        info.error = message;
        return info;
    }
    if (!stream.Skip(infoSize - consumed)) {
        info.error = "truncated cfg tail";
        return info;
    }

    // The effect list only exists from 0x0105 onwards. For 0x0101..0x0104 the loader reads
    // the header struct and STOPS --
    //     SFile.ReadBuffer ( &OldAnimInfo, sizeof(SANIMCONINFO_104) );
    // with no dwSize and no per-effect payloads (DxSkinAniManSaveLoad.cpp).
    // CONFIRMED against authentic data: 689 files of 0x0104 are exactly 840 bytes, 10 of
    // 0x0102 exactly 564 and 10 of 0x0103 exactly 732 -- the file ends at the header block.
    if (version < 0x0105) {
        info.effectCount = 0;
        info.effectsSkipped = false;
        info.valid = true;
        return info;
    }

    uint32_t effectCount = 0;
    if (!stream.ReadU32(effectCount)) {
        char message[200];
        std::snprintf(message, sizeof(message),
                      "cfg 0x%04X is truncated: the %u-byte header block is followed by no "
                      "effect count", version, infoSize);
        info.error = message;
        return info;
    }
    info.effectCount = effectCount;
    info.effectsSkipped = (effectCount != 0);
    // Per-effect payloads (DxEffAniData::LoadFile) and the 0x0107 trailing DxAniScale block
    // are not decoded: that needs the effect readers. Nothing after this point is used.

    info.valid = true;
    return info;
}

void EP7AnimationReader::BindToSkeleton(EP7AnimClip& clip, EP7Skeleton& skeleton) {
    auto bind = [&skeleton](std::vector<EP7BoneTrack>& tracks) {
        for (auto& track : tracks) {
            track.boneIndex = skeleton.FindBoneIndex(track.boneName);
        }
    };
    bind(clip.mainTracks);
    bind(clip.upBodyTracks);
}

// ---------------------------------------------------------------------------
// EP7AnimationPlayer
// ---------------------------------------------------------------------------

void EP7AnimationPlayer::Play(const EP7AnimClip* clip, const EP7AnimInfo* info) {
    m_clip = clip;
    m_info = info;
    m_ended = false;
    m_time = 0.0f;
    if (info) {
        // ANIMCONTNODE::FrameMove clamps the clock to [m_dwSTime, m_dwETime].
        m_time = static_cast<float>(info->startTime);
        m_loop = info->IsLoop();
    } else {
        m_loop = true;
    }
}

void EP7AnimationPlayer::Stop() {
    m_clip = nullptr;
    m_info = nullptr;
    m_time = 0.0f;
    m_ended = false;
}

bool EP7AnimationPlayer::Advance(float deltaSeconds) {
    if (!m_clip) return false;
    if (deltaSeconds < 0.0f) deltaSeconds = 0.0f;

    // ANIMCONTNODE::FrameMove / DxSkinAniControl::FrameMove:
    //     m_fTime += fElapsedTime * UNITANIKEY_PERSEC;      // 4800 keys per second
    m_time += deltaSeconds * static_cast<float>(kUnitAniKeyPerSec);

    const float startF = m_info ? static_cast<float>(m_info->startTime) : 0.0f;
    float endF = m_info ? static_cast<float>(m_info->endTime) : 0.0f;
    if (!m_info || endF <= startF) {
        // No usable window in metadata: fall back to the key-track length.
        endF = static_cast<float>(m_clip->MaxKeyTime());
        if (endF <= startF) return false;
    }

    if (m_time < startF) m_time = startF;

    // ANIMCONTNODE::FrameMove also reports bEndAnim as soon as the clock enters the last
    // two unit intervals of a non-looping clip:
    //     if (fCurTime > (m_dwETime - (m_UNITTIME*2))) { if (!(flag & ACF_LOOP)) bEndAnim = TRUE; }
    if (m_info && !m_loop) {
        const float unit = static_cast<float>(m_info->unitTime);
        if (unit > 0.0f && m_time > (endF - unit * 2.0f)) m_ended = true;
    }

    if (m_time > endF) {
        if (m_loop) {
            // ANIMCONTNODE::FrameMove: wrap back to m_dwSTime.
            const float span = endF - startF;
            if (span > 0.0f) {
                m_time = startF + std::fmod(m_time - startF, span);
            } else {
                m_time = startF;
            }
        } else {
            m_time = endF;
            m_ended = true;
        }
    }
    return !m_ended;
}

void EP7AnimationPlayer::ApplyToSkeleton(EP7Skeleton& skeleton) const {
    if (!m_clip) return;

    const uint32_t time = static_cast<uint32_t>(m_time > 0.0f ? m_time : 0.0f);

    auto applyTracks = [&skeleton, time](const std::vector<EP7BoneTrack>& tracks) {
        for (const auto& track : tracks) {
            if (track.boneIndex < 0) continue;
            EP7Bone& bone = const_cast<EP7Bone&>(skeleton.GetBone(static_cast<size_t>(track.boneIndex)));

            if (!track.matrixKeys.empty()) {
                // SAnimation::SetMatrix — snap to the first key with dwTime >= t.
                const size_t found = ep7anim::LowerBoundKey(track.matrixKeys, time);
                const size_t index = ep7anim::SampleKeyIndex(found, track.matrixKeys.size());
                const EP7MatrixKey& key = track.matrixKeys[index];
                for (int r = 0; r < 4; ++r) {
                    for (int c = 0; c < 4; ++c) {
                        bone.local.m[r][c] = key.mat[r * 4 + c];
                    }
                }
                bone.localIsFromSqt = false;
                continue;
            }

            if (!track.quatPosKeys.empty()) {
                // SetPosQuat — snap to the key and decompress the quaternion.
                const size_t found = ep7anim::LowerBoundKey(track.quatPosKeys, time);
                const size_t index = ep7anim::SampleKeyIndex(found, track.quatPosKeys.size());
                const EP7QuatPosKey& key = track.quatPosKeys[index];
                bone.scale = ep7math::Vec3(key.scale[0], key.scale[1], key.scale[2]);
                bone.position = ep7math::Vec3(key.pos[0], key.pos[1], key.pos[2]);
                bone.rotation = ep7math::DecompressQuaternion(key.quatComp0, key.quatComp1);
                if (bone.rotation.x == 0.0f && bone.rotation.y == 0.0f && bone.rotation.z == 0.0f &&
                    bone.rotation.w == 0.0f) {
                    bone.rotation.w = 1.0f;  // degenerate compressed key: keep a valid rotation
                }
                bone.localIsFromSqt = true;
                continue;
            }

            // Explicit S * R * T composition (SAnimation::SetTime else-branch).
            ep7math::Mat4 result = ep7math::Mat4::Identity();
            bool animated = false;

            if (!track.scaleKeys.empty()) {
                const uint32_t period = track.scaleKeys.back().time;
                const float fTime = (period > 0) ? std::fmod(static_cast<float>(time), static_cast<float>(period))
                                                 : static_cast<float>(time);
                size_t i1 = 0;
                size_t i2 = 0;
                for (size_t k = 0; k < track.scaleKeys.size(); ++k) {
                    if (static_cast<float>(track.scaleKeys[k].time) > fTime) {
                        i2 = k;
                        i1 = (k > 0) ? (k - 1) : 0;
                        break;
                    }
                }
                const float t1 = static_cast<float>(track.scaleKeys[i1].time);
                const float t2 = static_cast<float>(track.scaleKeys[i2].time);
                const float lerp = (t2 - t1) == 0.0f ? 0.0f : (fTime - t1) / (t2 - t1);
                const ep7math::Vec3 a(track.scaleKeys[i1].s[0], track.scaleKeys[i1].s[1], track.scaleKeys[i1].s[2]);
                const ep7math::Vec3 b(track.scaleKeys[i2].s[0], track.scaleKeys[i2].s[1], track.scaleKeys[i2].s[2]);
                const ep7math::Vec3 v = ep7math::Lerp(a, b, lerp);
                result = ep7math::Multiply(result, ep7math::Scaling(v.x, v.y, v.z));
                animated = true;
            }

            if (!track.rotateKeys.empty()) {
                const uint32_t period = track.rotateKeys.back().time;
                const float fTime = (period > 0) ? std::fmod(static_cast<float>(time), static_cast<float>(period))
                                                 : static_cast<float>(time);
                size_t i1 = 0;
                size_t i2 = 0;
                for (size_t k = 0; k < track.rotateKeys.size(); ++k) {
                    if (static_cast<float>(track.rotateKeys[k].time) > fTime) {
                        i2 = k;
                        i1 = (k > 0) ? (k - 1) : 0;
                        break;
                    }
                }
                const float t1 = static_cast<float>(track.rotateKeys[i1].time);
                const float t2 = static_cast<float>(track.rotateKeys[i2].time);
                const float lerp = (t2 - t1) == 0.0f ? 0.0f : (fTime - t1) / (t2 - t1);
                const auto& ka = track.rotateKeys[i1];
                const auto& kb = track.rotateKeys[i2];

                ep7math::Quat quat;
                if (ka.time == kb.time) {
                    quat = ep7math::Quat(ka.q[0], ka.q[1], ka.q[2], ka.q[3]);
                } else {
                    // USE_SQUAD: D3DXQuaternionSquadSetup over i0,i1,i2,i3 with wraparound,
                    // approximated by the equivalent slerp when neighbours are unavailable.
                    const size_t count = track.rotateKeys.size();
                    const size_t i0 = (i1 == 0) ? (count - 1) : (i1 - 1);
                    size_t i3 = (i2 + 1) % count;
                    if (i0 == i2 || i3 == i1) {
                        quat = ep7math::Slerp(ep7math::Quat(ka.q[0], ka.q[1], ka.q[2], ka.q[3]),
                                              ep7math::Quat(kb.q[0], kb.q[1], kb.q[2], kb.q[3]), lerp);
                    } else {
                        const auto& k0 = track.rotateKeys[i0];
                        const auto& k3 = track.rotateKeys[i3];
                        (void)k0;
                        (void)k3;
                        // SQUAD is only a C1-continuous refinement of SLERP here; the
                        // original's D3DXQuaternionSquadSetup tangents are not reproduced.
                        quat = ep7math::Slerp(ep7math::Quat(ka.q[0], ka.q[1], ka.q[2], ka.q[3]),
                                              ep7math::Quat(kb.q[0], kb.q[1], kb.q[2], kb.q[3]), lerp);
                    }
                }
                // The original negates w after sampling (quat.w = -quat.w).
                quat.w = -quat.w;
                result = ep7math::Multiply(result, ep7math::RotationQuaternion(quat));
                animated = true;
            }

            if (!track.positionKeys.empty()) {
                const uint32_t period = track.positionKeys.back().time;
                const float fTime = (period > 0) ? std::fmod(static_cast<float>(time), static_cast<float>(period))
                                                 : static_cast<float>(time);
                size_t i1 = 0;
                size_t i2 = 0;
                for (size_t k = 0; k < track.positionKeys.size(); ++k) {
                    if (static_cast<float>(track.positionKeys[k].time) > fTime) {
                        i2 = k;
                        i1 = (k > 0) ? (k - 1) : 0;
                        break;
                    }
                }
                const float t1 = static_cast<float>(track.positionKeys[i1].time);
                const float t2 = static_cast<float>(track.positionKeys[i2].time);
                const float lerp = (t2 - t1) == 0.0f ? 0.0f : (fTime - t1) / (t2 - t1);
                const auto& pa = track.positionKeys[i1];
                const auto& pb = track.positionKeys[i2];
                const ep7math::Vec3 v = ep7math::Lerp(ep7math::Vec3(pa.p[0], pa.p[1], pa.p[2]),
                                                      ep7math::Vec3(pb.p[0], pb.p[1], pb.p[2]), lerp);
                result = ep7math::Multiply(result, ep7math::Translation(v.x, v.y, v.z));
                animated = true;
            } else if (animated) {
                // No position keys: the original re-uses the bone's rest translation.
                result = ep7math::Multiply(
                    result, ep7math::Translation(bone.localRest.m[3][0], bone.localRest.m[3][1], bone.localRest.m[3][2]));
            }

            if (animated) {
                bone.local = result;
                bone.localIsFromSqt = false;
            }
        }
    };

    applyTracks(m_clip->upBodyTracks);
    applyTracks(m_clip->mainTracks);
}

float EP7AnimationPlayer::NormalizedTime() const {
    if (!m_info) return 0.0f;
    const float start = static_cast<float>(m_info->startTime);
    const float end = static_cast<float>(m_info->endTime);
    if (end <= start) return 0.0f;
    float t = (m_time - start) / (end - start);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t;
}

}  // namespace RANMobile
