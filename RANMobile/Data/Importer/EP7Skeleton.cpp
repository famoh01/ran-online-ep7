#include "EP7Skeleton.h"

#include <cstring>

#include "EP7XFile.h"

namespace RANMobile {
namespace {

using ep7xfile::Token;

struct FrameRecord {
    std::string name;        // empty => nameless frame, dropped by the original loader
    int32_t parent = -1;     // index into the frame list, -1 => child of "__ROOT"
    EP7BodyPart body = EP7BodyPart::Default;
    bool hasMatrix = false;
    float matrix[16] = {0};  // FrameTransformMatrix, row-major
};

}  // namespace

// ---------------------------------------------------------------------------
// EP7Skeleton
// ---------------------------------------------------------------------------

void EP7Skeleton::Clear() {
    m_valid = false;
    m_error.clear();
    m_bones.clear();
    m_roots.clear();
    m_paletteOrder.clear();
    m_nameToIndex.clear();
}

int32_t EP7Skeleton::AddBone(const std::string& name, int32_t parent, EP7BodyPart body) {
    const int32_t index = static_cast<int32_t>(m_bones.size());
    EP7Bone bone;
    bone.name = name;
    bone.parent = parent;
    bone.body = body;
    m_bones.push_back(std::move(bone));
    auto it = m_nameToIndex.find(name);
    if (it == m_nameToIndex.end()) {
        m_nameToIndex.emplace(name, index);
    }
    return index;
}

// DxBoneTrans::AddBoneTrans inserts at the head of the sibling list:
//   if (!firstChild) firstChild = node;
//   else { node->sibling = firstChild->sibling; firstChild->sibling = node; }
// i.e. the *first* child stays the head and later siblings are appended after it.
void EP7Skeleton::RebuildLinks() {
    for (auto& bone : m_bones) {
        bone.firstChild = -1;
        bone.nextSibling = -1;
    }

    std::vector<int32_t> head(m_bones.size(), -1);
    for (int32_t i = 0; i < static_cast<int32_t>(m_bones.size()); ++i) {
        const int32_t parent = m_bones[i].parent;
        if (parent < 0 || parent >= static_cast<int32_t>(m_bones.size())) continue;
        if (head[static_cast<size_t>(parent)] < 0) {
            head[static_cast<size_t>(parent)] = i;
        } else {
            const int32_t prevNext = m_bones[static_cast<size_t>(head[static_cast<size_t>(parent)])].nextSibling;
            m_bones[static_cast<size_t>(i)].nextSibling = prevNext;
            m_bones[static_cast<size_t>(head[static_cast<size_t>(parent)])].nextSibling = i;
        }
    }
    for (int32_t p = 0; p < static_cast<int32_t>(m_bones.size()); ++p) {
        m_bones[static_cast<size_t>(p)].firstChild = head[static_cast<size_t>(p)];
    }

    // Depth-first traversal order == DxSkeleton::UpdateBones recursion order.
    m_paletteOrder.clear();
    m_paletteOrder.reserve(m_bones.size());
    std::vector<int32_t> stack;
    for (auto it = m_roots.rbegin(); it != m_roots.rend(); ++it) stack.push_back(*it);
    while (!stack.empty()) {
        const int32_t index = stack.back();
        stack.pop_back();
        m_paletteOrder.push_back(index);
        std::vector<int32_t> kids;
        int32_t child = m_bones[static_cast<size_t>(index)].firstChild;
        while (child >= 0) {
            kids.push_back(child);
            child = m_bones[static_cast<size_t>(child)].nextSibling;
        }
        for (auto it = kids.rbegin(); it != kids.rend(); ++it) stack.push_back(*it);
    }
}

bool EP7Skeleton::LoadFromXFile(const std::vector<uint8_t>& data) {
    return LoadFromXFile(data.data(), data.size());
}

bool EP7Skeleton::LoadFromXFile(const uint8_t* data, size_t size) {
    Clear();

    std::vector<Token> tokens;
    std::string lexError;
    if (!ep7xfile::Tokenize(data, size, tokens, lexError)) {
        m_error = lexError;
        return false;
    }

    size_t pos = 0;
    int frameCount = 0;
    std::vector<FrameRecord> frames;

    // DxSkeleton::LoadFile inserts the synthetic "__ROOT" bone before enumerating.
    const int32_t rootIndex = AddBone(kRootName, -1, EP7BodyPart::Default);
    m_roots.push_back(rootIndex);

    std::vector<int32_t> frameStack;
    bool malformed = false;
    std::string malformedReason;

    auto skipBalancedBlock = [&tokens, &pos]() -> bool {
        if (pos >= tokens.size() || tokens[pos].kind != Token::Kind::LBrace) return false;
        int depth = 0;
        while (pos < tokens.size()) {
            if (tokens[pos].kind == Token::Kind::LBrace) ++depth;
            if (tokens[pos].kind == Token::Kind::RBrace) {
                --depth;
                if (depth == 0) {
                    ++pos;
                    return true;
                }
            }
            ++pos;
        }
        return false;
    };

    while (pos < tokens.size() && !malformed) {
        const Token& t = tokens[pos];

        if (t.kind == Token::Kind::Semicolon) {
            ++pos;
            continue;
        }

        // Template declarations must be skipped structurally so their braces cannot be
        // mistaken for data objects:  template Frame { <22guid...> }
        if (t.kind == Token::Kind::Identifier && t.text == "template") {
            ++pos;
            while (pos < tokens.size() && tokens[pos].kind != Token::Kind::LBrace &&
                   tokens[pos].kind != Token::Kind::Semicolon) {
                ++pos;
            }
            if (pos < tokens.size() && tokens[pos].kind == Token::Kind::LBrace) {
                skipBalancedBlock();
            } else if (pos < tokens.size() && tokens[pos].kind == Token::Kind::Semicolon) {
                ++pos;
            }
            continue;
        }

        if (t.kind == Token::Kind::Identifier && t.text == "Frame") {
            ++pos;
            std::string name;
            bool haveName = false;
            if (pos < tokens.size() && tokens[pos].kind == Token::Kind::String) {
                name = tokens[pos].text;
                haveName = true;
                ++pos;
            } else if (pos < tokens.size() && tokens[pos].kind == Token::Kind::Identifier) {
                name = tokens[pos].text;
                haveName = true;
                ++pos;
            }
            if (pos >= tokens.size() || tokens[pos].kind != Token::Kind::LBrace) {
                malformed = true;
                malformedReason = "Frame without '{'";
                break;
            }
            ++pos;

            FrameRecord rec;
            rec.name = haveName ? name : std::string();
            rec.parent = frameStack.empty() ? -1 : frameStack.back();
            rec.body = (haveName && name == kUpBodyBoneName) ? EP7BodyPart::UpBody : EP7BodyPart::Default;
            frames.push_back(rec);
            frameStack.push_back(static_cast<int32_t>(frames.size()) - 1);
            if (haveName) ++frameCount;
            continue;
        }

        if (t.kind == Token::Kind::Identifier && t.text == "FrameTransformMatrix") {
            ++pos;
            if (pos >= tokens.size() || tokens[pos].kind != Token::Kind::LBrace) {
                malformed = true;
                malformedReason = "FrameTransformMatrix without '{'";
                break;
            }
            ++pos;
            float values[16] = {0};
            int read = 0;
            while (pos < tokens.size() && read < 16) {
                if (tokens[pos].kind == Token::Kind::Number) {
                    values[read++] = static_cast<float>(tokens[pos].number);
                    ++pos;
                } else if (tokens[pos].kind == Token::Kind::RBrace) {
                    break;
                } else {
                    ++pos;
                }
            }
            if (read != 16) {
                malformed = true;
                malformedReason = "FrameTransformMatrix needs exactly 16 floats";
                break;
            }
            // Consume to the matching '}'.
            int depth = 1;
            while (pos < tokens.size() && depth > 0) {
                if (tokens[pos].kind == Token::Kind::LBrace) ++depth;
                if (tokens[pos].kind == Token::Kind::RBrace) --depth;
                ++pos;
            }

            if (!frameStack.empty()) {
                FrameRecord& rec = frames[static_cast<size_t>(frameStack.back())];
                if (!rec.name.empty()) {
                    std::memcpy(rec.matrix, values, sizeof(values));
                    rec.hasMatrix = true;
                }
            }
            continue;
        }

        if (t.kind == Token::Kind::RBrace) {
            if (!frameStack.empty()) frameStack.pop_back();
            ++pos;
            continue;
        }

        ++pos;
    }

    if (malformed) {
        Clear();
        m_error = std::string("malformed .x: ") + malformedReason;
        return false;
    }
    if (frameCount == 0) {
        Clear();
        m_error = "no named Frame objects found in .x skeleton";
        return false;
    }

    // Materialise bones in file order (the original inserts into BoneMap in the same
    // depth-first order, so name lookups resolve identically).
    std::vector<int32_t> frameToBone(frames.size(), -1);
    for (size_t i = 0; i < frames.size(); ++i) {
        FrameRecord& rec = frames[i];
        if (rec.name.empty()) continue;  // dropped by DxBoneCollector

        int32_t parentBone = rootIndex;
        if (rec.parent >= 0) {
            int32_t p = rec.parent;
            while (p >= 0 && frames[static_cast<size_t>(p)].name.empty()) p = frames[static_cast<size_t>(p)].parent;
            if (p >= 0 && frameToBone[static_cast<size_t>(p)] >= 0) parentBone = frameToBone[static_cast<size_t>(p)];
        }

        // EMBODY propagation mirrors the original recursion: the parent's body part is
        // passed down, and "Bip01_Spine1" switches the whole subtree to EMBODY_UPBODY.
        EP7BodyPart body = EP7BodyPart::Default;
        if (rec.parent >= 0 && frames[static_cast<size_t>(rec.parent)].body == EP7BodyPart::UpBody) {
            body = EP7BodyPart::UpBody;
        }
        if (rec.name == kUpBodyBoneName) body = EP7BodyPart::UpBody;
        rec.body = body;

        const int32_t boneIndex = AddBone(rec.name, parentBone, body);
        frameToBone[i] = boneIndex;

        if (rec.hasMatrix) {
            ep7math::Mat4 mat;
            for (int r = 0; r < 4; ++r) {
                for (int c = 0; c < 4; ++c) mat.m[r][c] = rec.matrix[r * 4 + c];
            }
            m_bones[static_cast<size_t>(boneIndex)].localRest = mat;
        }
    }

    RebuildLinks();

    // DxBoneCollector.cpp:362 — bind pose SQT: CUR = ORIG = PREV = D3DXMatrixToSQT(matRot).
    for (auto& bone : m_bones) {
        ep7math::MatrixToSQT(bone.localRest, bone.restScale, bone.restRotation, bone.restPosition);
    }

    m_valid = true;
    ResetToRestPose();
    return true;
}

EP7Bone* EP7Skeleton::FindBone(const std::string& name) {
    auto it = m_nameToIndex.find(name);
    if (it == m_nameToIndex.end()) return nullptr;
    return &m_bones[static_cast<size_t>(it->second)];
}

int32_t EP7Skeleton::FindBoneIndex(const std::string& name) const {
    auto it = m_nameToIndex.find(name);
    if (it == m_nameToIndex.end()) return -1;
    return it->second;
}

void EP7Skeleton::ResetToRestPose() {
    // DxBoneTrans::ResetBone (DxBoneCollector.cpp:34, USE_ANI_QUATERNION branch):
    //   m_dwFlag = 0; m_fScale = 1; m_vQuatMIX = identity;
    //   m_pQuatPosCUR = m_pQuatPosORIG; m_pQuatPosPREV = m_pQuatPosORIG;
    // (plus the non-quaternion matRot = matRotOrig, which is `local = localRest` here).
    for (auto& bone : m_bones) {
        bone.local = bone.localRest;
        bone.localIsFromSqt = false;
        bone.scale = bone.restScale;
        bone.position = bone.restPosition;
        bone.rotation = bone.restRotation;
        bone.prevScale = bone.restScale;
        bone.prevPosition = bone.restPosition;
        bone.prevRotation = bone.restRotation;
        bone.useBlending = false;
        bone.useAniAdd = false;
        bone.mixRotation = ep7math::Quat(0.0f, 0.0f, 0.0f, 1.0f);
        bone.aniScale = 1.0f;
    }
}

void EP7Skeleton::UpdateBones(const ep7math::Mat4& matRoot, float blendWeight) {
    for (int32_t root : m_roots) {
        UpdateBonesRecursive(root, matRoot, blendWeight);
    }
}

void EP7Skeleton::UpdateBonesRecursive(int32_t index, const ep7math::Mat4& parentCombined, float blendWeight) {
    if (index < 0 || index >= static_cast<int32_t>(m_bones.size())) return;
    EP7Bone& bone = m_bones[static_cast<size_t>(index)];

    // DxSkeleton::UpdateBones, USE_ANI_QUATERNION path (DxBoneCollector.cpp:71+):
    //   1. base SQuatPos = m_pQuatPosCUR
    //   2. if (m_dwFlag & USE_ANI_BLENDING): lerp/slerp PREV -> CUR by g_fWeight
    //   3. if (m_dwFlag & USE_ANI_ADD): vQuat = m_vQuatMIX * vQuat (both normalized)
    //   4. local matrix = RotationQuaternion(quat), rows scaled by per-axis scale * m_fScale,
    //      translation = pos
    //   5. matCombined = local * matParent
    if (bone.localIsFromSqt) {
        ep7math::Vec3 scale = bone.scale;
        ep7math::Vec3 position = bone.position;
        ep7math::Quat rotation = bone.rotation;

        if (bone.useBlending) {
            // D3DXVec3Lerp + D3DXQuaternionSlerp against m_pQuatPosPREV with g_fWeight.
            const float w = (blendWeight < 0.0f) ? 0.0f : ((blendWeight > 1.0f) ? 1.0f : blendWeight);
            scale = ep7math::Lerp(bone.prevScale, bone.scale, w);
            position = ep7math::Lerp(bone.prevPosition, bone.position, w);
            rotation = ep7math::Slerp(bone.prevRotation, bone.rotation, w);
        }

        if (bone.useAniAdd) {
            // D3DXQuaternionNormalize both, then vQuat = m_vQuatMIX * vQuat.
            rotation = ep7math::Multiply(ep7math::Normalize(bone.mixRotation), ep7math::Normalize(rotation));
        }

        ep7math::Mat4 rot = ep7math::RotationQuaternion(rotation);
        for (int c = 0; c < 3; ++c) {
            rot.m[0][c] *= scale.x * bone.aniScale;
            rot.m[1][c] *= scale.y * bone.aniScale;
            rot.m[2][c] *= scale.z * bone.aniScale;
        }
        rot.m[3][0] = position.x;
        rot.m[3][1] = position.y;
        rot.m[3][2] = position.z;
        bone.local = rot;
    }

    bone.combined = ep7math::Multiply(bone.local, parentCombined);

    int32_t child = bone.firstChild;
    while (child >= 0) {
        const int32_t next = m_bones[static_cast<size_t>(child)].nextSibling;
        UpdateBonesRecursive(child, bone.combined, blendWeight);
        child = next;
    }
}

}  // namespace RANMobile
