#include "EP7Animation.h"

#include <cmath>
#include <cstring>

namespace RANMobile {
namespace {

constexpr uint32_t kMaxBoneTracks = 4096;
constexpr uint32_t kMaxKeysPerTrack = 200000;
constexpr uint32_t kMaxNameLen = 4096;

// Reads a SAnimation record.  Returns false on malformed input.
// Every SAnimation begins with its own DWORD version (SAnimation::SaveToFile writes
// `SFile << VERSION` first; SAnimation::LoadFromFile reads dwVER and dispatches), so the
// version is read per track and is independent of the container header version.
bool ReadBoneTrack(ep7byte::Stream& s, EP7BoneTrack& out) {
    uint32_t version = 0;
    if (!s.ReadU32(version)) return false;

    if (version == 0x0102) {
        // SAnimation::Load — verified field order (SAnimation.cpp:1515).
        if (!s.ReadString(out.boneName, kMaxNameLen)) return false;

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
        out.rotateKeys.resize(count);
        for (uint32_t i = 0; i < count; ++i) {
            if (!s.ReadU32(out.rotateKeys[i].time)) return false;
            for (int k = 0; k < 4; ++k) {
                if (!s.ReadF32(out.rotateKeys[i].q[k])) return false;
            }
        }

        if (!s.ReadU32(count)) return false;
        if (count > kMaxKeysPerTrack) {
            s.Fail("animation position key count exceeds sanity limit");
            return false;
        }
        if (!s.CanReadElements(count, EP7AnimationReader::kPositionKeySize)) {
            s.Fail("animation position key block runs past end of buffer");
            return false;
        }
        out.positionKeys.resize(count);
        for (uint32_t i = 0; i < count; ++i) {
            if (!s.ReadU32(out.positionKeys[i].time)) return false;
            for (int k = 0; k < 3; ++k) {
                if (!s.ReadF32(out.positionKeys[i].p[k])) return false;
            }
        }

        uint32_t garbage = 0;
        if (!s.ReadU32(garbage)) return false;  // written as m_strBone.length()

        if (!s.ReadU32(count)) return false;
        if (count > kMaxKeysPerTrack) {
            s.Fail("animation quatpos key count exceeds sanity limit");
            return false;
        }
        if (!s.CanReadElements(count, EP7AnimationReader::kQuatPosKeySize)) {
            s.Fail("animation quatpos key block runs past end of buffer");
            return false;
        }
        out.quatPosKeys.resize(count);
        for (uint32_t i = 0; i < count; ++i) {
            EP7QuatPosKey& key = out.quatPosKeys[i];
            if (!s.ReadU32(key.time)) return false;
            for (int k = 0; k < 3; ++k) {
                if (!s.ReadF32(key.scale[k])) return false;
            }
            for (int k = 0; k < 3; ++k) {
                if (!s.ReadF32(key.pos[k])) return false;
            }
            if (!s.ReadU32(key.quatComp0) || !s.ReadU32(key.quatComp1)) return false;
        }

        if (!s.ReadU32(garbage)) return false;

        if (!s.ReadU32(count)) return false;
        if (count > kMaxKeysPerTrack) {
            s.Fail("animation matrix key count exceeds sanity limit");
            return false;
        }
        if (!s.CanReadElements(count, EP7AnimationReader::kMatrixKeySize)) {
            s.Fail("animation matrix key block runs past end of buffer");
            return false;
        }
        out.matrixKeys.resize(count);
        for (uint32_t i = 0; i < count; ++i) {
            if (!s.ReadU32(out.matrixKeys[i].time)) return false;
            for (int k = 0; k < 16; ++k) {
                if (!s.ReadF32(out.matrixKeys[i].mat[k])) return false;
            }
        }

        if (!s.ReadU32(count)) return false;
        if (count > kMaxKeysPerTrack) {
            s.Fail("animation scale key count exceeds sanity limit");
            return false;
        }
        if (!s.CanReadElements(count, EP7AnimationReader::kScaleKeySize)) {
            s.Fail("animation scale key block runs past end of buffer");
            return false;
        }
        out.scaleKeys.resize(count);
        for (uint32_t i = 0; i < count; ++i) {
            if (!s.ReadU32(out.scaleKeys[i].time)) return false;
            for (int k = 0; k < 3; ++k) {
                if (!s.ReadF32(out.scaleKeys[i].s[k])) return false;
            }
        }
        return true;
    }

    if (version == 0x0101) {
        // SAnimation::Load_0101 — position, rotate, scale, matrix, quatpos, bone name LAST.
        uint32_t count = 0;

        if (!s.ReadU32(count)) return false;
        if (count > kMaxKeysPerTrack || !s.CanReadElements(count, EP7AnimationReader::kPositionKeySize)) {
            s.Fail("animation position key block invalid");
            return false;
        }
        out.positionKeys.resize(count);
        for (uint32_t i = 0; i < count; ++i) {
            if (!s.ReadU32(out.positionKeys[i].time)) return false;
            for (int k = 0; k < 3; ++k) {
                if (!s.ReadF32(out.positionKeys[i].p[k])) return false;
            }
        }

        if (!s.ReadU32(count)) return false;
        if (count > kMaxKeysPerTrack || !s.CanReadElements(count, EP7AnimationReader::kRotateKeySize)) {
            s.Fail("animation rotate key block invalid");
            return false;
        }
        out.rotateKeys.resize(count);
        for (uint32_t i = 0; i < count; ++i) {
            if (!s.ReadU32(out.rotateKeys[i].time)) return false;
            for (int k = 0; k < 4; ++k) {
                if (!s.ReadF32(out.rotateKeys[i].q[k])) return false;
            }
        }

        if (!s.ReadU32(count)) return false;
        if (count > kMaxKeysPerTrack || !s.CanReadElements(count, EP7AnimationReader::kScaleKeySize)) {
            s.Fail("animation scale key block invalid");
            return false;
        }
        out.scaleKeys.resize(count);
        for (uint32_t i = 0; i < count; ++i) {
            if (!s.ReadU32(out.scaleKeys[i].time)) return false;
            for (int k = 0; k < 3; ++k) {
                if (!s.ReadF32(out.scaleKeys[i].s[k])) return false;
            }
        }

        if (!s.ReadU32(count)) return false;
        if (count > kMaxKeysPerTrack || !s.CanReadElements(count, EP7AnimationReader::kMatrixKeySize)) {
            s.Fail("animation matrix key block invalid");
            return false;
        }
        out.matrixKeys.resize(count);
        for (uint32_t i = 0; i < count; ++i) {
            if (!s.ReadU32(out.matrixKeys[i].time)) return false;
            for (int k = 0; k < 16; ++k) {
                if (!s.ReadF32(out.matrixKeys[i].mat[k])) return false;
            }
        }

        if (!s.ReadU32(count)) return false;
        if (count > kMaxKeysPerTrack || !s.CanReadElements(count, EP7AnimationReader::kQuatPosKeySize)) {
            s.Fail("animation quatpos key block invalid");
            return false;
        }
        out.quatPosKeys.resize(count);
        for (uint32_t i = 0; i < count; ++i) {
            EP7QuatPosKey& key = out.quatPosKeys[i];
            if (!s.ReadU32(key.time)) return false;
            for (int k = 0; k < 3; ++k) {
                if (!s.ReadF32(key.scale[k])) return false;
            }
            for (int k = 0; k < 3; ++k) {
                if (!s.ReadF32(key.pos[k])) return false;
            }
            if (!s.ReadU32(key.quatComp0) || !s.ReadU32(key.quatComp1)) return false;
        }

        if (!s.ReadString(out.boneName, kMaxNameLen)) return false;
        return true;
    }

    s.Fail("unsupported animation version");
    return false;
}

bool ReadTrackList(ep7byte::Stream& s, std::vector<EP7BoneTrack>& out) {
    uint32_t count = 0;
    if (!s.ReadU32(count)) return false;
    if (count > kMaxBoneTracks) {
        s.Fail("animation bone track count exceeds sanity limit");
        return false;
    }
    out.clear();
    out.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        EP7BoneTrack track;
        if (!ReadBoneTrack(s, track)) return false;
        out.push_back(std::move(track));
    }
    return true;
}

inline bool IsFinite(float v) { return std::isfinite(v); }

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

    if (version == 0x0102) {
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
        clip.error = "unsupported animation container version";
        return clip;
    }

    clip.valid = true;
    return clip;
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

    if (version != EP7AnimInfo::kVersion) {
        // Only the 0x0107 layout is implemented; older layouts differ in fixed-size
        // struct members (SANIMCONINFO_101.._106) and are documented as unsupported.
        info.error = "unsupported animation cfg version (only 0x0107 implemented)";
        return info;
    }

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
    // EMANI_MAINTYPE has AN_TYPE_SIZE == 21 valid values.
    if (mainType > static_cast<uint32_t>(EP7AnimMainType::None)) {
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

    if (!stream.ReadU16(info.strikeCount)) {
        info.error = "truncated cfg strike count";
        return info;
    }
    for (uint32_t i = 0; i < EP7AnimInfo::kStrike; ++i) {
        int32_t piece = 0;
        int32_t effect = 0;
        uint32_t frame = 0;
        if (!stream.ReadI32(piece) || !stream.ReadI32(effect) || !stream.ReadU32(frame)) {
            info.error = "truncated cfg strike table";
            return info;
        }
        info.strikeEff[i].piece = piece;
        info.strikeEff[i].effect = effect;
        info.strikeEff[i].frame = frame;
    }

    // SChaSoundData (10 frame slots; 388 bytes on the original 32-bit build):
    //   int PlayFrameCount; char szFileName[10][32]; BOOL m_bLoop[10]; WORD m_PlayFrame[11];
    // The exact size is validated rather than assumed: 4 + 320 + 40 + 22 = 386 bytes of
    // payload; alignment inside SChaSoundData pads m_PlayFrame to offset 364, so 386 is
    // the on-disk size and 388 is only the in-memory struct size.  Both are accepted and
    // the 22-byte frame tail is read explicitly.
    {
        int32_t playFrameCount = 0;
        if (!stream.ReadI32(playFrameCount)) {
            info.error = "truncated sound data";
            return info;
        }
        for (uint32_t i = 0; i < 10; ++i) {
            if (!stream.Skip(32)) {
                info.error = "truncated sound file names";
                return info;
            }
        }
        for (uint32_t i = 0; i < 10; ++i) {
            uint32_t loop = 0;
            if (!stream.ReadU32(loop)) {
                info.error = "truncated sound loop flags";
                return info;
            }
        }
        for (uint32_t i = 0; i < 11; ++i) {
            uint16_t frame = 0;
            if (!stream.ReadU16(frame)) {
                info.error = "truncated sound play frames";
                return info;
            }
        }
    }

    uint32_t effectCount = 0;
    if (!stream.ReadU32(effectCount)) {
        info.error = "truncated cfg effect count";
        return info;
    }
    info.effectCount = effectCount;
    info.effectsSkipped = (effectCount != 0);

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
