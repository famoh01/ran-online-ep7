//
// EP7SkinAniControl.h — Authentic EP7 character animation state machine (Phase 3J B17)
//
// SOURCE OF TRUTH (enginelib/Meshs/, all referenced functions read in full):
//   DxSkinAniControl.h/.cpp   — ANIMCONTNODE, DxAniControl, DxAniControlCORE,
//                               DxSkinAniControl (SELECTANI / DEFAULTANI / ClassifyAnimation
//                               / FrameMove / Render pose pipeline)
//   SAnimation.cpp            — SAnimContainer::UpdateTime (EMBODY switch),
//                               SetPosQuat (bFirst), SetMix, UpdateBoneScale
//   DxBoneCollector.cpp       — DxBoneTrans::ResetBone, DxSkeleton::UpdateBones
//                               (USE_ANI_QUATERNION path, g_fWeight blending)
//
// WHAT IS IMPLEMENTED (verified behaviour only):
//   * Animation registry sorted by name and classified by (MainType, SubType)
//     (DxSkinAniControl::LoadAnimation + ClassifyAnimation, ANI_MAX=6 per bucket).
//   * SELECTANI(MType, SType, dwSelect): missing bucket or out-of-range dwSelect marks
//     the current animation ended immediately (m_bEndAnim=TRUE) and falls back to the
//     first registered clip when nothing is playing; a hit routes through
//     DxAniControlCORE::SetAnimation, which sends ACF_UPBODY clips to the up-body
//     control and everything else to the down-body control.
//   * SetAnimation restart semantics: re-selecting the current clip is a no-op
//     unless the skill-reset flag is set; a switch backs the previous clip into the
//     layer control (time preserved), resets the clock to m_dwSTime and clears
//     m_bEndAnim (DxAniControl::SetAnimation).
//   * FrameMove: m_fTime += fElapsedTime * UNITANIKEY_PERSEC (4800), then the
//     ANIMCONTNODE::FrameMove window logic (start clamp, non-loop end at
//     ETime - UNITTIME*2, wrap to STime for loops) implemented by EP7AnimationPlayer.
//   * Transition blending: m_fWeight ramps toward 1 at 7 units/second
//     (DxSkinAniControl::FrameMove), EMANI_ENDFREEZE forces it to 1; the layer
//     controls write the PREV pose (SetPosQuat bFirst=FALSE) and the main controls
//     write the CUR pose (bFirst=TRUE); DxSkeleton::UpdateBones lerps PREV->CUR by
//     g_fWeight when USE_ANI_BLENDING is raised. Layers are frozen at
//     ETime - UNITTIME once ended (FrameMoveLayer) and cleared at weight >= 1
//     (DxAniControlCORE::UpdateTime).
//   * End handling: DxAniControl::Validate clears a control whose non-loop clip
//     reported its end; the game layer then re-selects (DEFAULTANI / SELECTANI).
//   * Shock-mix overlay (DxAniControlCORE::SetMixAni / FrameMoveMIX / UpdateMix):
//     plays an AN_SHOCK_MIX clip on top; SetMix raises USE_ANI_ADD with
//     m_vQuatMIX = quat(key@t) * inverse(quat(key0)) per quatPos-keyed track.
//
// WHAT IS DELIBERATELY NOT IMPLEMENTED (documented gaps, no invented behaviour):
//   * Threaded .bin loading (m_rAnimThreadLoad / AddBinFIleLoad): the mobile loader
//     parses synchronously off the render thread; the swap-in branch of
//     DxAniControl::FrameMove therefore never fires.
//   * Character sound cues (ANIMCONTNODE::ChaSound) — the port has no audio path yet.
//   * m_wDivFrame stop points with bContinue=FALSE (gameplay-driven early stop):
//     the control exposes bContinue but the mobile gameplay layer always passes TRUE.
//   * BIGHEAD/BIGHAND cosmetic bone scaling (m_bBIGHEAD defaults to false).
//   * The non-USE_ANI_QUATERNION build of UpdateTime (the shipped client defines
//     USE_ANI_QUATERNION — DxSkinDefine.h:3; B17 CI evidence check-run verifies it
//     in the EP7-era sources as well).
//
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../Data/Importer/EP7Animation.h"

namespace RANMobile {

// DxAniControl — one control channel. The ANIMCONTNODE clock is an
// EP7AnimationPlayer (same FrameMove timing semantics); endAnim is m_bEndAnim.
struct EP7AniControl {
    EP7AnimationPlayer player;
    bool endAnim = false;

    void CleanUp();  // DxAniControl::CleanUp
    bool HasNode() const { return player.IsPlaying(); }

    // DxAniControl::SetAnimation — backups the current node into `backup` (time
    // preserved), restarts at m_dwSTime. Returns FALSE when the same clip is already
    // current (unless `resetSkillAni`) or the clip is not valid (IsValidateANI).
    bool SetAnimation(const EP7AnimClip* clip, const EP7AnimInfo* info, EP7AniControl& backup,
                      bool resetSkillAni = false);

    // DxAniControl::FrameMove (without the threaded-load swap-in; see header notes).
    void FrameMove(float elapsedSeconds, bool bContinue, bool endFreeze);

    // DxAniControl::FrameMoveLayer — freeze at ETime - UNITTIME once ended.
    void FrameMoveLayer(float elapsedSeconds, bool bContinue, bool endFreeze);

    // DxAniControl::FrameMoveMIX — pure clock, node cleared past m_dwETime.
    void FrameMoveMix(float elapsedSeconds);

    // DxAniControl::Validate — drop the node when the clip reported its end.
    void Validate();

    void UpdateTime(EP7Skeleton& skeleton, bool bFirst, EP7EmBody emBody) const;  // UpdateTime
    void UpdateBoneScale(EP7Skeleton& skeleton, float weight, bool bFirst) const; // UpdateBoneScale
};

// DxAniControlCORE — the up-body / down-body / mix control set with their blend layers.
struct EP7AniControlCore {
    EP7AniControl upBody;        // m_sUpBodyAC
    EP7AniControl upBodyLayer;   // m_sUpBodyAC_Layer
    EP7AniControl downBody;      // m_sDownBodyAC
    EP7AniControl downBodyLayer; // m_sDownBodyAC_Layer
    EP7AniControl mix;           // m_sMixAC

    void CleanUp();
    void FrameMove(float elapsedSeconds, bool bContinue, bool endFreeze, float weight);
    // DxAniControlCORE::UpdateTime, USE_ANI_QUATERNION branch.
    void UpdateTime(float weight, EP7Skeleton& skeleton);
    void UpdateBoneScale(float weight, EP7Skeleton& skeleton) const;
    void UpdateMix(EP7Skeleton& skeleton) const;
    // Routes by the ACF_UPBODY flag (DxAniControlCORE::SetAnimation).
    bool SetAnimation(const EP7AnimClip* clip, const EP7AnimInfo* info, bool resetSkillAni = false);
    void SetMixAni(const EP7AnimClip* clip, const EP7AnimInfo* info);
};

// DxSkinAniControl — the per-character facade: registry + classification + selection.
class EP7SkinAniControl {
public:
    void Reset();  // ClearAll

    // DxSkinAniControl::LoadAnimation (registry insert) + ClassifyAnimation.
    // `name` is the registry key (the authentic m_szName from the .cfg; the caller
    // passes the .chf animation name when the .cfg is unavailable).
    bool AddAnimation(const EP7AnimClip* clip, const EP7AnimInfo* info, const std::string& name);
    void ClassifyAnimation();

    // SELECTANI(szAnim) — by registry name.
    bool SelectAnimation(const std::string& clipName);
    // SELECTANI(MType, SType, dwFlags, dwSelect).
    bool SelectAnimation(int32_t mainType, int32_t subType, uint32_t select = 0);
    // Direct index selection (mobile convenience; routes through SetAnimation).
    bool SelectAnimationIndex(int32_t clipIndex);

    void DefaultAnimation();  // DEFAULTANI

    // DOSHOCKMIX (first AN_SHOCK_MIX == main type 11 clip; authentic picks a random one,
    // the mobile preview always uses the first — deterministic, documented).
    void DoShockMix();
    bool IsShockMix() const { return m_core.mix.HasNode(); }

    void FrameMove(float elapsedSeconds, bool bContinue = true, bool endFreeze = false);
    void UpdateTime(EP7Skeleton& skeleton);            // DxSkinAniControl::Render pose part
    void UpdateBoneScale(EP7Skeleton& skeleton) const;
    void UpdateMix(EP7Skeleton& skeleton) const;
    void ResetBone(EP7Skeleton& skeleton) const;       // DxSkinAniControl::Render reset part

    bool IsEndAnim() const;                            // ISENDANIM
    // GETCURTIME (DxSkinAniControl): the down-body control's animation clock.
    float CurrentTime() const { return m_core.downBody.player.CurrentTime(); }
    // TRUE only when a down-body clip is playing AND reported its end
    // (the Validate()-cleared state reports IsEndAnim()==true with no node).
    bool CurrentEnded() const { return m_core.downBody.HasNode() && m_core.downBody.endAnim; }
    float Weight() const { return m_weight; }
    size_t AnimationCount() const { return m_nodes.size(); }

    const EP7AnimInfo* CurrentInfo() const;
    const EP7AnimClip* CurrentClip() const;
    int32_t CurrentClipIndex() const { return m_currentIndex; }
    int32_t CurrentUpBodyClipIndex() const { return m_currentUpBodyIndex; }
    // GETCURMTYPE / GETCURSTYPE (-1 when nothing plays).
    int32_t CurrentMainType() const;
    int32_t CurrentSubType() const;
    bool UpBodyActive() const { return m_core.upBody.HasNode(); }

private:
    struct Node {
        const EP7AnimClip* clip = nullptr;
        const EP7AnimInfo* info = nullptr;
        std::string name;
        int32_t mainType = -1;
        int32_t subType = -1;
    };

    const Node* FindByName(const std::string& clipName) const;  // findanicontnode
    int32_t IndexOf(const EP7AnimClip* clip) const {
        for (size_t i = 0; i < m_nodes.size(); ++i) {
            if (m_nodes[i].clip == clip) return static_cast<int32_t>(i);
        }
        return -1;
    }

    std::vector<Node> m_nodes;  // m_vecAnim, kept sorted by name (authentic std::sort)
    // m_vecAniType[mainType][subType]: bucket key = mainType * 64 + subType (both < 50).
    static constexpr uint32_t kAniTypeSize = 64;
    std::unordered_map<uint32_t, std::vector<uint32_t>> m_byType;

    EP7AniControlCore m_core;
    float m_weight = 1.0f;
    bool m_endFreeze = false;   // EMANI_ENDFREEZE
    int32_t m_currentIndex = -1;
    int32_t m_currentUpBodyIndex = -1;
};

}  // namespace RANMobile
