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

// EMANI_MAINTYPE (enginelib/Meshs/DxAniKeys.h) — values are file data, never renumber.
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
    None = 21,
    TypeSize = 21,
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
    static constexpr uint32_t kVersion106 = 0x0106;
    static constexpr uint32_t kFirstLayout = 0x0101;  // SANIMCONINFO_101
    static constexpr uint32_t kNameSize = 128;   // ACF_SZNAME
    static constexpr uint32_t kDiv = 8;          // ACF_DIV
    static constexpr uint32_t kStrike = 9;       // ACF_STRIKE

    uint32_t version = 0;   // header fileId — set even when the layout is unsupported
    bool unsupported = false;  // a real SANIMCONINFO file, but of a version this build
                               // has no definition for (see BLOCKERS.md B11). This is a
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

    // Parses SANIMCONINFO (.cfg version 0x0107). Stops at the effect list.
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
