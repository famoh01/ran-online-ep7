//
// EP7SkinAniControl.cpp — Authentic EP7 character animation state machine (B17).
// See EP7SkinAniControl.h for the source-of-truth mapping. Every method cites the
// enginelib function it reproduces.
//
#include "EP7SkinAniControl.h"

#include <algorithm>

namespace RANMobile {
namespace {
// ANI_MAX (DxSkinAniControl.h): each (MainType, SubType) bucket holds at most 6 clips.
constexpr size_t kAniMaxPerType = 6;
constexpr uint32_t kAniTypeSize = 64;
constexpr int32_t kShockMixMainType = 11;  // AN_SHOCK_MIX

bool BucketKey(int32_t mainType, int32_t subType, uint32_t& outKey) {
    if (mainType < 0 || mainType >= 34) return false;   // AN_TYPE_SIZE
    if (subType < 0 || subType >= 50) return false;     // AN_SUB_00_SIZE
    outKey = static_cast<uint32_t>(mainType) * kAniTypeSize + static_cast<uint32_t>(subType);
    return true;
}

}  // namespace

// ---------------------------------------------------------------------------
// EP7AniControl (DxAniControl)
// ---------------------------------------------------------------------------

void EP7AniControl::CleanUp() {
    // DxAniControl::CleanUp: prevKeyTime=-1, fTime=0, bEndAnim=FALSE, node=NULL.
    player.Stop();
    endAnim = false;
}

bool EP7AniControl::SetAnimation(const EP7AnimClip* clip, const EP7AnimInfo* info, EP7AniControl& backup,
                                 bool resetSkillAni) {
    // DxAniControl::SetAnimation (DxSkinAniControl.cpp:155).
    if (player.GetClip() == clip && !resetSkillAni) return false;
    if (!clip || !info || !clip->valid) return false;  // !pNode->pAnimCont / IsValidateANI

    // sAniControlBACKUP: previous clock state preserved.
    backup.player.Play(player.GetClip(), player.GetInfo());
    backup.player.SetTime(player.CurrentTime());
    backup.endAnim = endAnim;

    player.Play(clip, info);   // m_rAnimNode=pNode; m_fTime=m_dwSTime; StartAnim()
    endAnim = false;
    return true;
}

void EP7AniControl::FrameMove(float elapsedSeconds, bool bContinue, bool endFreeze) {
    // DxAniControl::FrameMove (DxSkinAniControl.cpp:58) + ANIMCONTNODE::FrameMove
    // (DxSkinAniControl.cpp:909) — the window logic lives in EP7AnimationPlayer::Advance.
    (void)bContinue;   // div-frame stop points: gameplay layer always continues (see header)
    if (!player.IsPlaying()) return;
    endAnim = !player.Advance(elapsedSeconds);
    (void)endFreeze;   // bEndFreeze is applied inside Advance (non-loop clamp/wrap)
}

void EP7AniControl::FrameMoveLayer(float elapsedSeconds, bool bContinue, bool endFreeze) {
    // DxAniControl::FrameMoveLayer (DxSkinAniControl.cpp:87): a ended layer freezes at
    // m_dwETime - m_UNITTIME and keeps sampling there until it is cleared.
    if (!player.IsPlaying()) return;
    if (endAnim) {
        const float endTime = player.GetInfo() ? static_cast<float>(player.GetInfo()->endTime) : 0.0f;
        const float unit = player.GetInfo() ? static_cast<float>(player.GetInfo()->unitTime) : 0.0f;
        player.SetTime(endTime - unit);
        (void)bContinue;
        (void)endFreeze;
    } else {
        endAnim = !player.Advance(elapsedSeconds);
    }
}

void EP7AniControl::FrameMoveMix(float elapsedSeconds) {
    // DxAniControl::FrameMoveMIX (DxSkinAniControl.cpp:125): clock only; the node is
    // dropped once the clock passes m_dwETime.
    if (!player.IsPlaying()) return;
    player.Advance(elapsedSeconds);
    const float endTime = player.GetInfo() ? static_cast<float>(player.GetInfo()->endTime) : 0.0f;
    if (player.CurrentTime() > endTime) CleanUp();
}

void EP7AniControl::Validate() {
    // DxAniControl::Validate (DxSkinAniControl.cpp:197): endAnim -> node cleared.
    if (endAnim) CleanUp();
}

void EP7AniControl::UpdateTime(EP7Skeleton& skeleton, bool bFirst, EP7EmBody emBody) const {
    // DxAniControl::UpdateTime (DxSkinAniControl.cpp:104) ->
    // SANIMCONINFO::UpdateTime(m_fTime, fWeight, bFirst, emBody) — the pose write.
    if (!player.IsPlaying()) return;
    player.ApplyToSkeleton(skeleton, bFirst, emBody);
}

void EP7AniControl::UpdateBoneScale(EP7Skeleton& skeleton, float weight, bool bFirst) const {
    // DxAniControl::UpdateBoneScale (DxSkinAniControl.cpp:188).
    if (!player.IsPlaying()) return;
    player.ApplyBoneScale(skeleton, weight, bFirst);
}

// ---------------------------------------------------------------------------
// EP7AniControlCore (DxAniControlCORE)
// ---------------------------------------------------------------------------

void EP7AniControlCore::CleanUp() {
    upBody.CleanUp();
    upBodyLayer.CleanUp();
    downBody.CleanUp();
    downBodyLayer.CleanUp();
    mix.CleanUp();
}

void EP7AniControlCore::FrameMove(float elapsedSeconds, bool bContinue, bool endFreeze, float weight) {
    // DxAniControlCORE::FrameMove (DxSkinAniControl.cpp:219).
    upBody.FrameMove(elapsedSeconds, bContinue, endFreeze);
    downBody.FrameMove(elapsedSeconds, bContinue, endFreeze);
    upBodyLayer.FrameMoveLayer(elapsedSeconds, bContinue, endFreeze);
    downBodyLayer.FrameMoveLayer(elapsedSeconds, bContinue, endFreeze);
    upBody.Validate();
    if (weight >= 1.0f) {
        upBodyLayer.Validate();
        downBodyLayer.Validate();
    }
    mix.FrameMoveMix(elapsedSeconds);
}

void EP7AniControlCore::UpdateTime(float weight, EP7Skeleton& skeleton) {
    // DxAniControlCORE::UpdateTime, USE_ANI_QUATERNION branch (DxSkinAniControl.cpp:238).
    // fWeight arguments to SetPosQuat are ignored by the original (the blend weight is
    // applied in DxSkeleton::UpdateBones via g_fWeight) — reproduced by only passing
    // bFirst down. The EMBODY argument selects which track list the clip drives.
    if (upBody.HasNode()) {
        upBody.UpdateTime(skeleton, true, EP7EmBody::UpBody);
        upBodyLayer.UpdateTime(skeleton, false, EP7EmBody::UpBody);
        downBody.UpdateTime(skeleton, true, EP7EmBody::DownBody);
        downBodyLayer.UpdateTime(skeleton, false, EP7EmBody::DownBody);
    } else {
        downBody.UpdateTime(skeleton, true, EP7EmBody::Default);
        downBodyLayer.UpdateTime(skeleton, false, EP7EmBody::Default);
    }

    if (weight >= 1.0f) {
        // m_sUpBodyAC_Layer.CleanUp(); m_sDownBodyAC_Layer.CleanUp();
        upBodyLayer.CleanUp();
        downBodyLayer.CleanUp();
    }

    // DxAniControlCORE::UpdateTime tail (DxSkinAniControl.cpp:278): bone scale is
    // applied by all four controls, the layers with the inverse weight, BEFORE the
    // hierarchy update runs (DxSkinAniControl::Render order).
    upBody.UpdateBoneScale(skeleton, weight, true);
    upBodyLayer.UpdateBoneScale(skeleton, 1.0f - weight, false);
    downBody.UpdateBoneScale(skeleton, weight, true);
    downBodyLayer.UpdateBoneScale(skeleton, 1.0f - weight, false);
}

void EP7AniControlCore::UpdateBoneScale(float weight, EP7Skeleton& skeleton) const {
    // Kept for parity with the DxAniControl::UpdateBoneScale entry point; the pose
    // pipeline applies bone scale inside UpdateTime (authentic tail order).
    upBody.UpdateBoneScale(skeleton, weight, true);
    upBodyLayer.UpdateBoneScale(skeleton, 1.0f - weight, false);
    downBody.UpdateBoneScale(skeleton, weight, true);
    downBodyLayer.UpdateBoneScale(skeleton, 1.0f - weight, false);
}

void EP7AniControlCore::UpdateMix(EP7Skeleton& skeleton) const {
    // DxAniControlCORE::UpdateMix (DxSkinAniControl.cpp:290) -> SAnimContainer::UpdateMix.
    if (!mix.HasNode()) return;
    mix.player.ApplyMix(skeleton);
}

bool EP7AniControlCore::SetAnimation(const EP7AnimClip* clip, const EP7AnimInfo* info, bool resetSkillAni) {
    // DxAniControlCORE::SetAnimation (DxSkinAniControl.cpp:295): ACF_UPBODY clips play
    // on the up-body control, everything else on the down-body control.
    if (!clip || !info) return false;
    if (info->flag & kAnimFlagUpBody) {
        return upBody.SetAnimation(clip, info, upBodyLayer, resetSkillAni);
    }
    return downBody.SetAnimation(clip, info, downBodyLayer, resetSkillAni);
}

void EP7AniControlCore::SetMixAni(const EP7AnimClip* clip, const EP7AnimInfo* info) {
    // DxAniControl::SetMixAni (DxSkinAniControl.cpp:182): node set, clock at 0.
    if (!clip || !info) return;
    mix.player.Play(clip, info);
    mix.player.SetTime(0.0f);
    mix.endAnim = false;
}

// ---------------------------------------------------------------------------
// EP7SkinAniControl (DxSkinAniControl)
// ---------------------------------------------------------------------------

void EP7SkinAniControl::Reset() {
    // DxSkinAniControl::ClearAll (DxSkinAniControl.cpp:430).
    m_nodes.clear();
    m_byType.clear();
    m_core.CleanUp();
    m_weight = 1.0f;
    m_endFreeze = false;
    m_currentIndex = -1;
    m_currentUpBodyIndex = -1;
}

bool EP7SkinAniControl::AddAnimation(const EP7AnimClip* clip, const EP7AnimInfo* info, const std::string& name) {
    // DxSkinAniControl::LoadAnimation (DxSkinAniControl.cpp:676): registry insert,
    // duplicates ignored, then std::sort by name.
    if (!clip || !clip->valid) return false;
    for (const auto& node : m_nodes) {
        if (node.clip == clip) return true;  // already registered
    }
    Node node;
    node.clip = clip;
    node.info = info;
    node.name = name;
    if (info && info->valid) {
        node.mainType = info->mainType;
        node.subType = info->subType;
    }
    m_nodes.push_back(node);
    std::sort(m_nodes.begin(), m_nodes.end(),
              [](const Node& a, const Node& b) { return a.name < b.name; });
    ClassifyAnimation();
    return true;
}

void EP7SkinAniControl::ClassifyAnimation() {
    // DxSkinAniControl::ClassifyAnimation (DxSkinAniControl.cpp:702): buckets per
    // (MainType, SubType) in registry order (SVEC::insert caps at ANI_MAX entries).
    m_byType.clear();
    for (uint32_t i = 0; i < m_nodes.size(); ++i) {
        const Node& node = m_nodes[i];
        uint32_t key = 0;
        if (!BucketKey(node.mainType, node.subType, key)) continue;
        auto& bucket = m_byType[key];
        if (bucket.size() < kAniMaxPerType) bucket.push_back(static_cast<uint32_t>(i));
    }
}

const EP7SkinAniControl::Node* EP7SkinAniControl::FindByName(const std::string& clipName) const {
    // DxSkinAniControl::findanicontnode — the registry is sorted, so a binary search
    // would be authentic; the mobile registry is small, a linear scan is equivalent.
    for (const auto& node : m_nodes) {
        if (node.name == clipName) return &node;
    }
    return nullptr;
}

bool EP7SkinAniControl::SelectAnimation(const std::string& clipName) {
    // DxSkinAniControl::SELECTANI(szAnim, dwFlags) (DxSkinAniControl.cpp:509).
    const Node* node = FindByName(clipName);
    if (!node) return false;
    if (m_core.SetAnimation(node->clip, node->info)) {
        m_weight = 0.0f;  // m_fWeight = 0 (blend ramp restarts)
        const int32_t index = IndexOf(node->clip);
        if (node->info && (node->info->flag & kAnimFlagUpBody)) m_currentUpBodyIndex = index;
        else m_currentIndex = index;
    }
    return true;
}

bool EP7SkinAniControl::SelectAnimation(int32_t mainType, int32_t subType, uint32_t select) {
    // DxSkinAniControl::SELECTANI(MType, SType, dwFlags, dwSelect) (DxSkinAniControl.cpp:536).
    uint32_t key = 0;
    const bool bucketKnown = BucketKey(mainType, subType, key);
    auto it = bucketKnown ? m_byType.find(key) : m_byType.end();
    if (!bucketKnown || it == m_byType.end() || select >= it->second.size()) {
        // Missing bucket / out-of-range select: end the current animation immediately
        // and guarantee some node is current (m_vecAnim[0]).
        m_core.downBody.endAnim = true;
        if (!m_core.downBody.HasNode() && !m_nodes.empty()) {
            m_core.downBody.player.Play(m_nodes[0].clip, m_nodes[0].info);
            m_currentIndex = 0;
        }
        return false;
    }
    const uint32_t index = it->second[select];
    const Node& node = m_nodes[index];
    if (m_core.SetAnimation(node.clip, node.info)) {
        m_weight = 0.0f;
        if (node.info->flag & kAnimFlagUpBody) m_currentUpBodyIndex = static_cast<int32_t>(index);
        else m_currentIndex = static_cast<int32_t>(index);
    }
    return true;
}

bool EP7SkinAniControl::SelectAnimationIndex(int32_t clipIndex) {
    // Direct registry selection (mobile convenience path with SetAnimation semantics).
    if (clipIndex < 0 || static_cast<size_t>(clipIndex) >= m_nodes.size()) return false;
    const Node& node = m_nodes[static_cast<size_t>(clipIndex)];
    if (!m_core.SetAnimation(node.clip, node.info)) {
        // Same node already current: authentic SetAnimation reports FALSE and keeps
        // playing — that is success for the caller-level "select" intent (an invalid
        // clip also lands here, and HasNode() would be false then).
        return m_core.downBody.HasNode() || m_core.upBody.HasNode();
    }
    m_weight = 0.0f;
    if (node.info && (node.info->flag & kAnimFlagUpBody)) m_currentUpBodyIndex = clipIndex;
    else m_currentIndex = clipIndex;
    return true;
}

void EP7SkinAniControl::DefaultAnimation() {
    // DxSkinAniControl::DEFAULTANI (DxSkinAniControl.cpp:523): empty registry -> full
    // clean-up; otherwise play m_vecAnim[0].
    if (m_nodes.empty()) {
        m_core.CleanUp();
        m_currentIndex = -1;
        m_currentUpBodyIndex = -1;
        return;
    }
    const Node& node = m_nodes[0];
    if (m_core.SetAnimation(node.clip, node.info)) {
        m_weight = 0.0f;
        m_currentIndex = 0;
    }
}

void EP7SkinAniControl::DoShockMix() {
    // DxSkinAniControl::DOSHOCKMIX (DxSkinAniControl.cpp:639): first AN_SHOCK_MIX clip
    // onto the mix control (the original randomises; the mobile port is deterministic).
    if (IsShockMix()) return;
    uint32_t key = 0;
    if (!BucketKey(kShockMixMainType, 0, key)) return;
    auto it = m_byType.find(key);
    if (it == m_byType.end() || it->second.empty()) return;
    const Node& node = m_nodes[it->second[0]];
    m_core.SetMixAni(node.clip, node.info);
}

void EP7SkinAniControl::FrameMove(float elapsedSeconds, bool bContinue, bool endFreeze) {
    // DxSkinAniControl::FrameMove (DxSkinAniControl.cpp:795): weight ramp at 7/s,
    // EMANI_ENDFREEZE pins it to 1, then the core clock updates.
    if (endFreeze) {
        m_endFreeze = true;
    }
    if (m_endFreeze) {
        m_weight = 1.0f;
    } else {
        m_weight += elapsedSeconds * 7.0f;
        if (m_weight > 1.0f) m_weight = 1.0f;
        if (m_weight < 0.0f) m_weight = 0.0f;
    }
    m_core.FrameMove(elapsedSeconds, bContinue, m_endFreeze, m_weight);
}

void EP7SkinAniControl::UpdateTime(EP7Skeleton& skeleton) {
    // DxSkinAniControl::Render pose part (DxSkinAniControl.cpp:822): UpdateTime with
    // the current weight (the layer controls carry the outgoing pose).
    m_core.UpdateTime(m_weight, skeleton);
}

void EP7SkinAniControl::UpdateBoneScale(EP7Skeleton& skeleton) const {
    m_core.UpdateBoneScale(m_weight, skeleton);
}

void EP7SkinAniControl::UpdateMix(EP7Skeleton& skeleton) const {
    // DxSkinAniControl::Render: m_sAniControlCORE.UpdateMix().
    m_core.UpdateMix(skeleton);
}

void EP7SkinAniControl::ResetBone(EP7Skeleton& skeleton) const {
    // DxSkinAniControl::Render reset part (DxSkinAniControl.cpp:814): every bone back
    // to ORIG so un-keyed bones show the bind pose (DxBoneTrans::ResetBone).
    skeleton.ResetToRestPose();
}

bool EP7SkinAniControl::IsEndAnim() const {
    // DxSkinAniControl::ISENDANIM (DxSkinAniControl.cpp:407): TRUE when nothing plays
    // or the current down-body clip reported its end.
    if (!m_core.downBody.HasNode()) return true;
    return m_core.downBody.endAnim;
}

const EP7AnimInfo* EP7SkinAniControl::CurrentInfo() const {
    const EP7AnimClip* clip = CurrentClip();
    if (!clip) return nullptr;
    if (m_core.downBody.HasNode() && m_core.downBody.player.GetClip() == clip) {
        return m_core.downBody.player.GetInfo();
    }
    return nullptr;
}

const EP7AnimClip* EP7SkinAniControl::CurrentClip() const {
    if (m_core.downBody.HasNode()) return m_core.downBody.player.GetClip();
    return nullptr;
}

int32_t EP7SkinAniControl::CurrentMainType() const {
    const EP7AnimInfo* info = m_core.downBody.HasNode() ? m_core.downBody.player.GetInfo() : nullptr;
    return info ? info->mainType : -1;
}

int32_t EP7SkinAniControl::CurrentSubType() const {
    const EP7AnimInfo* info = m_core.downBody.HasNode() ? m_core.downBody.player.GetInfo() : nullptr;
    return info ? info->subType : -1;
}

}  // namespace RANMobile
