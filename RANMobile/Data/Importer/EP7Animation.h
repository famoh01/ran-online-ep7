//
// EP7Animation.h — Authentic EP7 animation container (.bin) + metadata (.cfg) reader
//                  and the verified sampling rules of the original engine (Phase 3J)
//
// SOURCE OF TRUTH
// ---------------
// enginelib/Meshs/SAnimation.h/.cpp
// enginelib/Meshs/DxSkinAniMan.cpp            (LoadAnimContainer / CreateAnimationData)
// enginelib/Meshs/DxSkinAniManSaveLoad.cpp    (SANIMCONINFO::LoadFile — the .cfg)
// enginelib/Meshs/DxSkinAniControl.cpp        (ANIMCONTNODE::FrameMove — playback timing)
// enginelib/Meshs/DxAniKeys.h                 (key structs, UNITANIKEY_PERSEC, AN_* enums)
// enginelib/Common/SerialFile.cpp             (byte layout of every primitive)
//
// FILE RELATIONSHIP (verified)
// ---------------------------
// For an animation whose .cfg is "walk.cfg", the clip data is "walk.bin"
//      (DxSkinAniMan::CreateAnimationData: STRUTIL::ChangeExt(name, ".x", strBinFile, ".bin"))
// and the source art was "walk.x".
// The .cfg is SANIMCONINFO (metadata: name, skeleton, frame windows, main/sub type,
// strike frames, sound cues).  The .bin is SAnimContainer:
//
//      [128-byte file type][DWORD fileId]           <- basestream header (ReadFileType)
//      DWORD fileVersion (= 0x0102)                 <- SAnimContainer::VERSION
//      DWORD upBodyCount;   upBodyCount   x SAnimation
//      DWORD mainCount;     mainCount     x SAnimation
//
// NOTE: for version 0x0101 the order/number of lists differs (SAnimContainer::Load_0101:
// main list first, then up-body list with no second count).  Both are handled.
//
// SAnimation::Load (version 0x0102) — exact field order from SAnimation.cpp:1515
//      string m_strBone                     (DWORD len+1, chars)
//      DWORD  rotateCount;   SRotateKey[]   { DWORD time; float q[4]; }       20 bytes each
//      DWORD  positionCount; SPositionKey[] { DWORD time; float p[3]; }       16 bytes each
//      DWORD  garbage                       (written as string length; ignored on load)
//      DWORD  quatPosCount;  SQuatPosKey[]  { DWORD t; float s[3]; float p[3];
//                                             DWORD q0; DWORD q1; }           36 bytes each
//      DWORD  garbage
//      DWORD  matrixCount;   SMatrixKey[]   { DWORD time; float mat[16]; }    80 bytes each
//                                             (D3DXMATRIXA16 is 16-byte aligned, so the
//                                              matrix starts at offset 16 and the struct is
//                                              16 + 64 = 80 bytes — verified with the real
//                                              legacy header in LegacyDX_Include)
//      DWORD  scaleCount;    SScaleKey[]    { DWORD time; float s[3]; }       16 bytes each
//
// SAMPLING RULES (reproduced exactly)
// ----------------------------------
//  * Matrix-keyed bones: Lower_bound() returns the FIRST key with dwTime >= t, where
//    t = fmod(globalTime, lastKey.dwTime).  The bone matrix is set, not interpolated.
//  * QuatPos-keyed bones: same Lower_bound on SQuatPosKey, and the pose is SNAPPED to
//    that key (no interpolation) — SetPosQuat only lerps between PREV and CUR when the
//    caller asks for blending (bFirst == FALSE), which the default playback path does
//    not.  Quaternion is decompressed with the 32767-centred 16-bit scheme.
//  * A bone with neither matrix nor quatpos keys is driven by explicit
//    rotate/position/scale keys composed as S * R * T, both slerped (SQUAD for
//    rotation, see USE_SQUAD) and lerped between neighbouring keys.
//  * ANIMCONTNODE::FrameMove clamps to [m_dwSTime, m_dwETime]; when the clip has no
//    ACF_LOOP flag it reports the animation as ended; looping clips wrap back to
//    m_dwSTime and reset the sound state.
//  * ACF_LOOP = 0x0010, ACF_NEWINFO = 0x0001, ACF_UPBODY = 0x0100,
//    ACF_DOWNBODY = 0x0200 (EMANICONINFO).
//
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "EP7ByteFormat.h"
#include "EP7Math.h"
#include "EP7Skeleton.h"

namespace RANMobile {

// EMANICONINFO (enginelib/Meshs/SAnimation.h)
enum EP7AnimFlag : uint32_t {
    kAnimFlagNewInfo = 0x0001,
    kAnimFlagLoop = 0x0010,
    kAnimFlagUpBody = 0x0100,
    kAnimFlagDownBody = 0x0200,
};

// EMANI_MAINTYPE — values are file data, never renumber.
//
// SOURCE (authentic EP7, verified for B16):
//   * backup repo matheomedrana/Ran-Online-EP7-Complete-Files @ tf661fbf,
//     "(Source Code) Ran Online Server Files and Developer Tools/Lib_Engine/Meshs/DxAniKeys.h"
//     (LFS SHA-256 9e4fbffdaf5746cf5cec81b5a43a0d42a0681569c7fdf77ecbc0db0a086c7559):
//     AN_GUARD_N=0..AN_GATHERING=20, AN_BIKE_A=21..AN_REARCAR_A=33, AN_NONE=34,
//     AN_TYPE_SIZE=34 (the old "//AN_TYPE_SIZE = 21" line is commented out there).
//   * The older local enginelib tree and [Lib]__Engine/DxMeshs/DxAniKeys.h
//     (Google-Drive EP7 drop) still carry AN_TYPE_SIZE=21 — the extended values 21..33
//     first appear in .cfg files of versions 0x0108..0x0115.
enum class EP7AnimMainType : int32_t {
    GuardN = 0,
    Placid = 1,
    Walk = 2,
    Run = 3,
    Attack = 4,
    Shock = 5,
    Die = 6,
    Create = 7,
    Talk = 8,
    Skill = 9,
    Gesture = 10,
    ShockMix = 11,
    GuardL = 12,
    ConftWin = 13,
    ConftLoss = 14,
    Spec = 15,
    SkillB = 16,
    SkillC = 17,
    SkillD = 18,
    SkillE = 19,
    Gathering = 20,
    // Extended EP7 range (AN_BIKE_A .. AN_REARCAR_A) present in 0x0108..0x0115 .cfg files.
    BikeA = 21,
    BikeB = 22,
    BikeC = 23,
    BikeEv1 = 24,
    Flight = 25,
    SkillF = 26,
    SkillG = 27,
    SkillH = 28,
    CarA = 29,
    CarB = 30,
    CarC = 31,
    CarD = 32,
    RearcarA = 33,
    None = 34,      // AN_NONE (the older engine tree had AN_NONE = 21)
    TypeSize = 34,  // AN_TYPE_SIZE
};

struct EP7RotateKey {
    uint32_t time = 0;
    float q[4] = {0, 0, 0, 1};
};

struct EP7PositionKey {
    uint32_t time = 0;
    float p[3] = {0, 0, 0};
};

struct EP7QuatPosKey {
    uint32_t time = 0;
    float scale[3] = {1, 1, 1};
    float pos[3] = {0, 0, 0};
    uint32_t quatComp0 = 0;
    uint32_t quatComp1 = 0;
};

// D3DXMATRIXA16 is 16-byte aligned in the original build: the matrix body starts at
// offset 16 inside SMatrixKey, and sizeof(SMatrixKey) == 80.
struct EP7MatrixKey {
    uint32_t time = 0;
    float mat[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};

struct EP7ScaleKey {
    uint32_t time = 0;
    float s[3] = {1, 1, 1};
};

// One SAnimation: the key tracks that drive a single bone.
struct EP7BoneTrack {
    std::string boneName;
    int32_t boneIndex = -1;  // resolved against the skeleton; -1 == bone not found
    std::vector<EP7RotateKey> rotateKeys;
    std::vector<EP7PositionKey> positionKeys;
    std::vector<EP7QuatPosKey> quatPosKeys;
    std::vector<EP7MatrixKey> matrixKeys;
    std::vector<EP7ScaleKey> scaleKeys;
};

// Metadata from the matching .cfg (SANIMCONINFO layout up to the effect list; the effect
// payload and the trailing DxAniScale block are intentionally not parsed).
//
// 0x0106 and 0x0107 share this prefix and differ only in that 0x0107 appends a DxAniScale
// block after the effect list (SANIMCONINFO::SaveFile). See DxSkinAniManSaveLoad.cpp.
struct EP7AnimInfo {
    static constexpr uint32_t kVersion = 0x0107;
    static constexpr uint32_t kVersionCfgLast = 0x0115;  // newest SANIMCONINFO the authentic loader implements
    static constexpr uint32_t kFirstLayout = 0x0101;  // SANIMCONINFO_101
    static constexpr uint32_t kNameSize = 128;   // ACF_SZNAME
    static constexpr uint32_t kDiv = 8;          // ACF_DIV
    static constexpr uint32_t kStrike = 9;       // ACF_STRIKE

    uint32_t version = 0;   // header fileId — set even when the layout is unsupported
    bool unsupported = false;  // a real SANIMCONINFO file, but of a version this build
                               // has no definition for (see BLOCKERS.md B11/B16). This is a
                               // deferred gap in coverage, NOT a defect in the reader.
    std::string fileType;   // 128-byte basestream type string, e.g. "SANIMCONINFO"
    std::string name;       // m_szName
    std::string skeleton;   // m_szSkeletion
    uint32_t flag = 0;
    uint32_t startTime = 0;      // m_dwSTime
    uint32_t endTime = 0;        // m_dwETime
    uint32_t endTimeOrig = 0;    // m_dwETimeOrig
    uint32_t unitTime = 0;       // m_UNITTIME
    int32_t mainType = 0;
    int32_t subType = 0;
    uint16_t divCount = 0;
    uint16_t divFrame[kDiv] = {0};
    uint16_t strikeCount = 0;
    // SANIMSTRIKE { EMPIECECHAR piece; EMSTRIKE_EFF effect; DWORD frame; }
    struct Strike {
        int32_t piece = 0;
        int32_t effect = 0;
        uint32_t frame = 0;
    } strikeEff[kStrike];
    uint32_t effectCount = 0;

    // -----------------------------------------------------------------
    // B16: versions 0x0108..0x0115 (Lib_Engine/Meshs/SAnimationInfoSaveLoad.cpp,
    // backup repo matheomedrana/Ran-Online-EP7-Complete-Files @ tf661fbf). The two
    // authentic loaders are marked "//official version partial read only" in
    // SAnimationInfo.h: the official client wrote trailing fields this loader never
    // reads, so files may legitimately carry bytes after the last authentic read.
    // -----------------------------------------------------------------

    // Length-prefixed companion names from the 0x0111..0x0115 layout. The authentic
    // loader reads szCfgFile and szBinFile and discards them (only szXFile ->
    // m_szName and szSkeleton -> m_szSkeletion are kept); they are recorded here
    // for forensics and .bin/.cfg pairing, and are empty for older versions.
    std::string cfgFileName;  // szCfgFile (0x0111+ only)
    std::string binFileName;  // szBinFile (0x0111+ only)

    // One m_listEffAni entry (DxEffAniData subclass), parsed per the authentic
    // DxEffAniSingle/Ghosting/Trace/FaceOff LoadFile layouts.
    struct CfgEffectAni {
        uint32_t typeId = 0;        // EMEFFANI_SINGLE/GHOSTING/TRACE/FACEOFF
        uint32_t effectVersion = 0; // inner property version (Single 0x0101, others 0x0100)
        uint32_t sizeBytes = 0;     // declared sizeof(property)
        uint32_t flags = 0;         // Single/Ghosting m_dwFlags; others 0
        uint32_t frame = 0;         // Single m_dwFrame; Ghosting m_dwFrame_1
        uint32_t frame2 = 0;        // Ghosting m_dwFrame_2
        float fact = 0.0f;          // Single m_fFact
        float scale = 0.0f;         // Single m_fScale (Trace m_fScale is not decoded)
        std::string name;           // Single m_szEff / FaceOff m_szTexture; empty otherwise
    };
    std::vector<CfgEffectAni> effects;  // populated for 0x0108+; 0x0105..0x0107 stop at the count

    // DxAniScale block (DxAniBoneScale.cpp): map of anim name -> scale-key list.
    // Key values are consumed for stream alignment but not retained; the entry names
    // and key counts are recorded.
    struct CfgAniScaleEntry {
        std::string animName;
        uint32_t keyCount = 0;
    };
    std::vector<CfgAniScaleEntry> aniScaleEntries;  // populated for 0x0108+

    // Bytes remaining after the last authentic read. The 0x0108..0x0115 loaders are
    // "//official version partial read only": trailing bytes are authentic client data
    // the loader never consumes. Trailing bytes are counted, never silently dropped,
    // and do NOT invalidate the file (the authentic LoadFile returns TRUE).
    size_t trailingBytes = 0;
    bool exactEof = false;  // true when the stream ends exactly at the last authentic field

    bool valid = false;
    bool effectsSkipped = false;
    std::string error;

    bool IsLoop() const { return (flag & kAnimFlagLoop) != 0; }
};

struct EP7AnimClip {
    std::vector<EP7BoneTrack> mainTracks;     // m_listAnimation
    std::vector<EP7BoneTrack> upBodyTracks;   // m_listAniUPBODY
    uint32_t version = 0;
    bool valid = false;
    std::string error;

    // Duration the sampler uses: the end time of the longest declared key track,
    // matching fmod(globalTime, lastKey.dwTime) in the original.
    uint32_t MaxKeyTime() const;
};

class EP7AnimationReader {
public:
    // Parses a SAnimContainer (.bin). `bytes` are the file bytes; when the asset came
    // out of a pack through CSerialMemory the 132-byte basestream header is present and
    // `hasHeader` must be true.
    static EP7AnimClip ParseContainer(const std::vector<uint8_t>& bytes, bool hasHeader, bool compByteEncoded);

    // Parses SANIMCONINFO (.cfg versions 0x0101..0x0107, and since B16 0x0108..0x0115
    // except the unused 0x0113). 0x0101..0x0104 end at the header block, 0x0105..0x0107
    // stop after the effect count; 0x0108..0x0115 are parsed to the end of the authentic
    // field sequence including the effect list and the DxAniScale block (see B16).
    static EP7AnimInfo ParseInfo(const std::vector<uint8_t>& bytes, bool hasHeader, bool compByteEncoded);

    // Resolve track bone names against a skeleton (original: pSkeleton->FindBone).
    static void BindToSkeleton(EP7AnimClip& clip, EP7Skeleton& skeleton);

    static constexpr size_t kRotateKeySize = 20;
    static constexpr size_t kPositionKeySize = 16;
    static constexpr size_t kQuatPosKeySize = 36;
    static constexpr size_t kMatrixKeySize = 80;
    static constexpr size_t kMatrixKeyAlignPadSize = 12;
    static constexpr size_t kScaleKeySize = 16;

    static_assert(sizeof(uint32_t) + kMatrixKeyAlignPadSize + 16 * sizeof(float) == kMatrixKeySize,
                  "SMatrixKey on-disk layout must sum to 80 bytes (4B time + 12B align16 pad + 64B mat)");
};

// Playback state for one clip instance (ANIMCONTNODE + fCurTime).
// DxSkinAniControl.cpp: `#define UNITANIKEY_PERSEC 4800` — animation clocks run in
// "key units" of 1/4800 s, so a delta of one second advances 4800 units.
inline constexpr uint32_t kUnitAniKeyPerSec = 4800;

class EP7AnimationPlayer {
public:
    void Play(const EP7AnimClip* clip, const EP7AnimInfo* info);
    void Stop();
    bool IsPlaying() const { return m_clip != nullptr; }

    // Returns false when a non-looping clip reached its end (bEndAnim in FrameMove).
    bool Advance(float deltaSeconds);

    // Applies the current time to every track (SAnimation::SetTime / SetMatrix).
    void ApplyToSkeleton(EP7Skeleton& skeleton) const;

    float CurrentTime() const { return m_time; }
    bool Loop() const { return m_loop; }
    void SetLoop(bool loop) { m_loop = loop; }
    // 0..1 normalized position inside [startTime, endTime]; 0 when the clip is empty.
    float NormalizedTime() const;

    const EP7AnimClip* GetClip() const { return m_clip; }
    const EP7AnimInfo* GetInfo() const { return m_info; }

private:
    const EP7AnimClip* m_clip = nullptr;
    const EP7AnimInfo* m_info = nullptr;
    float m_time = 0.0f;
    bool m_loop = true;
    bool m_ended = false;
};

// Single-track sampling helpers, exported for tests and reuse.
namespace ep7anim {

// Lower_bound over keys with dwTime >= time (SAnimation.cpp Lower_bound/Lower_bound3312).
template <typename KeyT>
size_t LowerBoundKey(const std::vector<KeyT>& keys, uint32_t time) {
    size_t lo = 0;
    size_t hi = keys.size();
    while (lo < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        if (keys[mid].time < time) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo;
}

// The original can return an index == count (when time >= last key) and then clamps
// by falling back to the last key.
inline size_t SampleKeyIndex(size_t found, size_t count) {
    if (count == 0) return 0;
    return (found >= count) ? (count - 1) : found;
}

}  // namespace ep7anim

}  // namespace RANMobile
