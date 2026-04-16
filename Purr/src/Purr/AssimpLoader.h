#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

struct MeshVertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
    // Skinned animation (pipeline stage 1): jusqu'a 4 influences.
    glm::ivec4 BoneIDs = glm::ivec4(-1, -1, -1, -1);
    glm::vec4  BoneWeights = glm::vec4(0.0f);
};

struct LoadedBone {
    std::string Name;
    glm::mat4   Offset = glm::mat4(1.0f);
};

struct LoadedMesh {
    std::vector<MeshVertex> Vertices;
    std::vector<uint32_t>   Indices;
    std::string             TexturePath; // si dispo dans le fichier
    // Skinned animation (pipeline stage 2): metadata du squelette par mesh.
    std::vector<LoadedBone> Bones;
};

struct LoadedNode {
    std::string Name;
    int         ParentIndex = -1;
    glm::mat4   LocalTransform = glm::mat4(1.0f);
};

struct LoadedPositionKey {
    float     Time = 0.0f;
    glm::vec3 Value = glm::vec3(0.0f);
};

struct LoadedRotationKey {
    float     Time = 0.0f;
    glm::quat Value = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
};

struct LoadedScaleKey {
    float     Time = 0.0f;
    glm::vec3 Value = glm::vec3(1.0f);
};

struct LoadedNodeAnimation {
    std::string                       NodeName;
    std::vector<LoadedPositionKey>    PositionKeys;
    std::vector<LoadedRotationKey>    RotationKeys;
    std::vector<LoadedScaleKey>       ScaleKeys;
};

struct LoadedAnimationClip {
    std::string                       Name;
    float                             DurationTicks = 0.0f;
    float                             TicksPerSecond = 0.0f;
    std::vector<LoadedNodeAnimation>  Channels;
};

struct LoadedAnimatedAsset {
    std::vector<LoadedMesh>           Meshes;
    std::vector<LoadedNode>           Nodes;
    std::vector<LoadedAnimationClip>  Animations;
};

struct AnimationPoseResult {
    // Transform global par noeud (meme index que LoadedAnimatedAsset::Nodes).
    std::vector<glm::mat4> GlobalNodeTransforms;
    // Matrices finales de skinning par bone name.
    std::unordered_map<std::string, glm::mat4> FinalBoneMatricesByName;
};

// Charge tous les meshes d'un fichier (OBJ, FBX, etc.)
std::vector<LoadedMesh> LoadMeshFile(const std::string& filepath);
// Charge meshes + hiérarchie + clips (pipeline animation).
LoadedAnimatedAsset LoadAnimatedAsset(const std::string& filepath);

// Evalue un clip a un temps donne (en secondes) et calcule les matrices globales.
AnimationPoseResult EvaluateAnimationPose(
    const LoadedAnimatedAsset& asset,
    const LoadedAnimationClip& clip,
    float timeSeconds);