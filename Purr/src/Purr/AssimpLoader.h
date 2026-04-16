#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>

struct MeshVertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
};

struct LoadedMesh {
    std::vector<MeshVertex> Vertices;
    std::vector<uint32_t>   Indices;
    std::string             TexturePath; // si dispo dans le fichier
};

// Charge tous les meshes d'un fichier (OBJ, FBX, etc.)
std::vector<LoadedMesh> LoadMeshFile(const std::string& filepath);