#pragma once

#include <memory>
#include <string>

namespace Purr {

    struct ObjVertex {
        float px, py, pz;
        float nx, ny, nz;
        float u, v;
    };

    // Centre le mesh et le scale pour qu'il rentre dans un cube de taille 1
    void NormalizeMesh(std::vector<ObjVertex>& verts);

    // Retourne nullptr si le fichier ne peut pas être ouvert
    std::shared_ptr<class VertexArray> LoadOBJ(const std::string& path, std::string& outTexPath);

}