#pragma once

#include <memory>
#include <string>
#include <vector>

namespace Purr {

    struct ObjVertex {
        float px, py, pz;
        float nx, ny, nz;
        float u, v;
    };

    struct ObjSubmesh {
        std::shared_ptr<class VertexArray> Mesh;
        std::string TexturePath;
        std::string MaterialName;
    };

    // Centre le mesh et le scale pour qu'il rentre dans un cube de taille 1
    void NormalizeMesh(std::vector<ObjVertex>& verts);

    // Retourne nullptr si le fichier ne peut pas être ouvert
    std::shared_ptr<class VertexArray> LoadOBJ(const std::string& path, std::string& outTexPath);
    bool LoadOBJMulti(const std::string& path, std::vector<ObjSubmesh>& outSubmeshes, std::string& outFirstTexPath);

}