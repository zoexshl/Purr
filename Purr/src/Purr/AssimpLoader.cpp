// AssimpLoader.cpp
#include "purrpch.h"
#include "AssimpLoader.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <filesystem>
#include <algorithm>
#include <unordered_map>

namespace
{
    constexpr int kMaxBoneInfluences = 4;

    glm::mat4 AiToGlm(const aiMatrix4x4& m)
    {
        glm::mat4 out(1.0f);
        out[0][0] = m.a1; out[1][0] = m.a2; out[2][0] = m.a3; out[3][0] = m.a4;
        out[0][1] = m.b1; out[1][1] = m.b2; out[2][1] = m.b3; out[3][1] = m.b4;
        out[0][2] = m.c1; out[1][2] = m.c2; out[2][2] = m.c3; out[3][2] = m.c4;
        out[0][3] = m.d1; out[1][3] = m.d2; out[2][3] = m.d3; out[3][3] = m.d4;
        return out;
    }

    void AddBoneInfluence(MeshVertex& v, int boneID, float weight)
    {
        if (weight <= 0.0f)
            return;

        for (int i = 0; i < kMaxBoneInfluences; ++i) {
            if (v.BoneIDs[i] < 0) {
                v.BoneIDs[i] = boneID;
                v.BoneWeights[i] = weight;
                return;
            }
        }

        int minIdx = 0;
        for (int i = 1; i < kMaxBoneInfluences; ++i) {
            if (v.BoneWeights[i] < v.BoneWeights[minIdx])
                minIdx = i;
        }
        if (weight > v.BoneWeights[minIdx]) {
            v.BoneIDs[minIdx] = boneID;
            v.BoneWeights[minIdx] = weight;
        }
    }

    void NormalizeBoneWeights(MeshVertex& v)
    {
        float sum = v.BoneWeights.x + v.BoneWeights.y + v.BoneWeights.z + v.BoneWeights.w;
        if (sum > 1e-6f)
            v.BoneWeights /= sum;
    }

    void BuildNodeHierarchyRecursive(const aiNode* node, int parentIndex, std::vector<LoadedNode>& outNodes)
    {
        if (!node)
            return;

        LoadedNode n;
        n.Name = node->mName.C_Str();
        n.ParentIndex = parentIndex;
        n.LocalTransform = AiToGlm(node->mTransformation);
        const int thisIndex = (int)outNodes.size();
        outNodes.push_back(n);

        for (unsigned int i = 0; i < node->mNumChildren; ++i)
            BuildNodeHierarchyRecursive(node->mChildren[i], thisIndex, outNodes);
    }

    glm::vec3 SamplePosition(const LoadedNodeAnimation& ch, float tTicks)
    {
        if (ch.PositionKeys.empty()) return glm::vec3(0.0f);
        if (ch.PositionKeys.size() == 1) return ch.PositionKeys.front().Value;
        if (tTicks <= ch.PositionKeys.front().Time) return ch.PositionKeys.front().Value;
        if (tTicks >= ch.PositionKeys.back().Time) return ch.PositionKeys.back().Value;
        for (size_t i = 0; i + 1 < ch.PositionKeys.size(); ++i) {
            const auto& a = ch.PositionKeys[i];
            const auto& b = ch.PositionKeys[i + 1];
            if (tTicks < a.Time || tTicks > b.Time) continue;
            float span = glm::max(b.Time - a.Time, 1e-6f);
            float alpha = glm::clamp((tTicks - a.Time) / span, 0.0f, 1.0f);
            return glm::mix(a.Value, b.Value, alpha);
        }
        return ch.PositionKeys.back().Value;
    }

    glm::quat SampleRotation(const LoadedNodeAnimation& ch, float tTicks)
    {
        if (ch.RotationKeys.empty()) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        if (ch.RotationKeys.size() == 1) return glm::normalize(ch.RotationKeys.front().Value);
        if (tTicks <= ch.RotationKeys.front().Time) return glm::normalize(ch.RotationKeys.front().Value);
        if (tTicks >= ch.RotationKeys.back().Time) return glm::normalize(ch.RotationKeys.back().Value);
        for (size_t i = 0; i + 1 < ch.RotationKeys.size(); ++i) {
            const auto& a = ch.RotationKeys[i];
            const auto& b = ch.RotationKeys[i + 1];
            if (tTicks < a.Time || tTicks > b.Time) continue;
            float span = glm::max(b.Time - a.Time, 1e-6f);
            float alpha = glm::clamp((tTicks - a.Time) / span, 0.0f, 1.0f);
            return glm::normalize(glm::slerp(glm::normalize(a.Value), glm::normalize(b.Value), alpha));
        }
        return glm::normalize(ch.RotationKeys.back().Value);
    }

    glm::vec3 SampleScale(const LoadedNodeAnimation& ch, float tTicks)
    {
        if (ch.ScaleKeys.empty()) return glm::vec3(1.0f);
        if (ch.ScaleKeys.size() == 1) return ch.ScaleKeys.front().Value;
        if (tTicks <= ch.ScaleKeys.front().Time) return ch.ScaleKeys.front().Value;
        if (tTicks >= ch.ScaleKeys.back().Time) return ch.ScaleKeys.back().Value;
        for (size_t i = 0; i + 1 < ch.ScaleKeys.size(); ++i) {
            const auto& a = ch.ScaleKeys[i];
            const auto& b = ch.ScaleKeys[i + 1];
            if (tTicks < a.Time || tTicks > b.Time) continue;
            float span = glm::max(b.Time - a.Time, 1e-6f);
            float alpha = glm::clamp((tTicks - a.Time) / span, 0.0f, 1.0f);
            return glm::mix(a.Value, b.Value, alpha);
        }
        return ch.ScaleKeys.back().Value;
    }
}

LoadedAnimatedAsset LoadAnimatedAsset(const std::string& filepath)
{
    Assimp::Importer importer;
    namespace fs = std::filesystem;
    std::string ext = fs::path(filepath).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    const bool isFbx = (ext == ".fbx");

    unsigned int flags =
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_CalcTangentSpace;

    // Roblox FBX semble deja avoir des UV compatibles, on ne flip pas.
    if (!isFbx)
        flags |= aiProcess_FlipUVs;

    PURR_INFO("Assimp import '{}' (isFbx={}, flipUVs={})", filepath, isFbx ? 1 : 0, (!isFbx) ? 1 : 0);
    const aiScene* scene = importer.ReadFile(filepath, flags);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        PURR_ERROR("Assimp: {0}", importer.GetErrorString());
        return {};
    }

    LoadedAnimatedAsset asset;
    asset.Meshes.reserve(scene->mNumMeshes);

    auto pickTexturePath = [](aiMaterial* mat) -> std::string
    {
        struct TexTry { aiTextureType Type; const char* Label; };
        const TexTry tries[] = {
            { aiTextureType_BASE_COLOR, "BASE_COLOR" },
            { aiTextureType_DIFFUSE,    "DIFFUSE"    },
            { aiTextureType_UNKNOWN,    "UNKNOWN"    },
        };

        aiString path;
        for (const auto& t : tries) {
            if (mat->GetTextureCount(t.Type) == 0)
                continue;
            if (mat->GetTexture(t.Type, 0, &path) == AI_SUCCESS) {
                std::string p = path.C_Str();
                if (p.rfind("*", 0) == 0) {
                    PURR_WARN("Assimp material texture '{}' de type {} est embedde/non-fichier (ignore).", p, t.Label);
                    continue;
                }
                PURR_INFO("Assimp material texture {}: {}", t.Label, p);
                return p;
            }
        }
        return "";
    };

    for (unsigned int m = 0; m < scene->mNumMeshes; m++)
    {
        aiMesh* mesh = scene->mMeshes[m];
        LoadedMesh loaded;

        for (unsigned int i = 0; i < mesh->mNumVertices; i++)
        {
            MeshVertex v;
            v.Position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
            if (mesh->mNormals)
                v.Normal = { mesh->mNormals[i].x,  mesh->mNormals[i].y,  mesh->mNormals[i].z };
            else
                v.Normal = { 0.0f, 1.0f, 0.0f };
            v.TexCoords = mesh->mTextureCoords[0]
                ? glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y)
                : glm::vec2(0.0f);
            loaded.Vertices.push_back(v);
        }

        if (mesh->HasBones()) {
            std::unordered_map<std::string, int> boneIndexByName;
            loaded.Bones.reserve(mesh->mNumBones);
            for (unsigned int bi = 0; bi < mesh->mNumBones; ++bi) {
                aiBone* bone = mesh->mBones[bi];
                if (!bone)
                    continue;
                const std::string boneName = bone->mName.C_Str();
                int boneIndex = -1;
                auto it = boneIndexByName.find(boneName);
                if (it == boneIndexByName.end()) {
                    boneIndex = (int)loaded.Bones.size();
                    boneIndexByName[boneName] = boneIndex;
                    LoadedBone lb;
                    lb.Name = boneName;
                    lb.Offset = AiToGlm(bone->mOffsetMatrix);
                    loaded.Bones.push_back(lb);
                }
                else {
                    boneIndex = it->second;
                }

                for (unsigned int wi = 0; wi < bone->mNumWeights; ++wi) {
                    const aiVertexWeight& w = bone->mWeights[wi];
                    if (w.mVertexId >= loaded.Vertices.size())
                        continue;
                    AddBoneInfluence(loaded.Vertices[w.mVertexId], boneIndex, w.mWeight);
                }
            }

            for (auto& v : loaded.Vertices)
                NormalizeBoneWeights(v);

            PURR_INFO("Assimp mesh {} -> bones: {}", m, (int)loaded.Bones.size());
        }

        for (unsigned int i = 0; i < mesh->mNumFaces; i++)
            for (unsigned int j = 0; j < mesh->mFaces[i].mNumIndices; j++)
                loaded.Indices.push_back(mesh->mFaces[i].mIndices[j]);

        if (!mesh->HasTextureCoords(0)) {
            PURR_WARN("Assimp mesh {} n'a pas d'UV (rendu texture limite).", m);
        }

        // Texture matiere (base color/diffuse)
        if (mesh->mMaterialIndex >= 0)
        {
            aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
            loaded.TexturePath = pickTexturePath(mat);
            PURR_INFO("Assimp mesh {} (mat {}) -> texture '{}'", m, mesh->mMaterialIndex, loaded.TexturePath);
        }
        else {
            PURR_WARN("Assimp mesh {} sans material index.", m);
        }

        asset.Meshes.push_back(loaded);
    }

    BuildNodeHierarchyRecursive(scene->mRootNode, -1, asset.Nodes);

    asset.Animations.reserve(scene->mNumAnimations);
    for (unsigned int ai = 0; ai < scene->mNumAnimations; ++ai) {
        const aiAnimation* anim = scene->mAnimations[ai];
        if (!anim)
            continue;

        LoadedAnimationClip clip;
        clip.Name = anim->mName.length > 0 ? anim->mName.C_Str() : ("Anim_" + std::to_string(ai));
        clip.DurationTicks = (float)anim->mDuration;
        clip.TicksPerSecond = (float)(anim->mTicksPerSecond > 0.0 ? anim->mTicksPerSecond : 30.0);
        clip.Channels.reserve(anim->mNumChannels);

        for (unsigned int ci = 0; ci < anim->mNumChannels; ++ci) {
            const aiNodeAnim* ch = anim->mChannels[ci];
            if (!ch)
                continue;

            LoadedNodeAnimation nodeAnim;
            nodeAnim.NodeName = ch->mNodeName.C_Str();
            nodeAnim.PositionKeys.reserve(ch->mNumPositionKeys);
            nodeAnim.RotationKeys.reserve(ch->mNumRotationKeys);
            nodeAnim.ScaleKeys.reserve(ch->mNumScalingKeys);

            for (unsigned int k = 0; k < ch->mNumPositionKeys; ++k) {
                LoadedPositionKey key;
                key.Time = (float)ch->mPositionKeys[k].mTime;
                key.Value = glm::vec3(
                    ch->mPositionKeys[k].mValue.x,
                    ch->mPositionKeys[k].mValue.y,
                    ch->mPositionKeys[k].mValue.z);
                nodeAnim.PositionKeys.push_back(key);
            }
            for (unsigned int k = 0; k < ch->mNumRotationKeys; ++k) {
                LoadedRotationKey key;
                key.Time = (float)ch->mRotationKeys[k].mTime;
                key.Value = glm::quat(
                    ch->mRotationKeys[k].mValue.w,
                    ch->mRotationKeys[k].mValue.x,
                    ch->mRotationKeys[k].mValue.y,
                    ch->mRotationKeys[k].mValue.z);
                nodeAnim.RotationKeys.push_back(key);
            }
            for (unsigned int k = 0; k < ch->mNumScalingKeys; ++k) {
                LoadedScaleKey key;
                key.Time = (float)ch->mScalingKeys[k].mTime;
                key.Value = glm::vec3(
                    ch->mScalingKeys[k].mValue.x,
                    ch->mScalingKeys[k].mValue.y,
                    ch->mScalingKeys[k].mValue.z);
                nodeAnim.ScaleKeys.push_back(key);
            }
            clip.Channels.push_back(std::move(nodeAnim));
        }

        PURR_INFO("Assimp anim {}: '{}' duration={} tps={} channels={}",
            ai, clip.Name, clip.DurationTicks, clip.TicksPerSecond, (int)clip.Channels.size());
        asset.Animations.push_back(std::move(clip));
    }

    return asset;
}

std::vector<LoadedMesh> LoadMeshFile(const std::string& filepath)
{
    return LoadAnimatedAsset(filepath).Meshes;
}

AnimationPoseResult EvaluateAnimationPose(
    const LoadedAnimatedAsset& asset,
    const LoadedAnimationClip& clip,
    float timeSeconds)
{
    AnimationPoseResult out;
    out.GlobalNodeTransforms.resize(asset.Nodes.size(), glm::mat4(1.0f));
    if (asset.Nodes.empty())
        return out;

    std::unordered_map<std::string, const LoadedNodeAnimation*> channelByNodeName;
    channelByNodeName.reserve(clip.Channels.size());
    for (const auto& ch : clip.Channels)
        channelByNodeName[ch.NodeName] = &ch;

    const float tps = (clip.TicksPerSecond > 0.0f) ? clip.TicksPerSecond : 30.0f;
    const float duration = glm::max(clip.DurationTicks, 0.0001f);
    float tTicks = timeSeconds * tps;
    tTicks = fmodf(tTicks, duration);
    if (tTicks < 0.0f) tTicks += duration;

    for (size_t i = 0; i < asset.Nodes.size(); ++i) {
        const LoadedNode& node = asset.Nodes[i];
        glm::mat4 local = node.LocalTransform;
        auto it = channelByNodeName.find(node.Name);
        if (it != channelByNodeName.end() && it->second) {
            const LoadedNodeAnimation& ch = *it->second;
            glm::vec3 p = SamplePosition(ch, tTicks);
            glm::quat r = SampleRotation(ch, tTicks);
            glm::vec3 s = SampleScale(ch, tTicks);
            local = glm::translate(glm::mat4(1.0f), p) * glm::mat4_cast(r) * glm::scale(glm::mat4(1.0f), s);
        }

        if (node.ParentIndex >= 0 && node.ParentIndex < (int)out.GlobalNodeTransforms.size())
            out.GlobalNodeTransforms[i] = out.GlobalNodeTransforms[node.ParentIndex] * local;
        else
            out.GlobalNodeTransforms[i] = local;
    }

    std::unordered_map<std::string, glm::mat4> nodeGlobalByName;
    nodeGlobalByName.reserve(asset.Nodes.size());
    for (size_t i = 0; i < asset.Nodes.size(); ++i)
        nodeGlobalByName[asset.Nodes[i].Name] = out.GlobalNodeTransforms[i];

    for (const auto& mesh : asset.Meshes) {
        for (const auto& bone : mesh.Bones) {
            auto it = nodeGlobalByName.find(bone.Name);
            if (it == nodeGlobalByName.end())
                continue;
            out.FinalBoneMatricesByName[bone.Name] = it->second * bone.Offset;
        }
    }

    return out;
}