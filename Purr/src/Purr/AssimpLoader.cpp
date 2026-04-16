// AssimpLoader.cpp
#include "purrpch.h"
#include "AssimpLoader.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

std::vector<LoadedMesh> LoadMeshFile(const std::string& filepath)
{
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(filepath,
        aiProcess_Triangulate |
        aiProcess_GenSmoothNormals |
        aiProcess_FlipUVs |           // OpenGL attend V inversé pour FBX
        aiProcess_CalcTangentSpace
    );

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        PURR_ERROR("Assimp: {0}", importer.GetErrorString());
        return {};
    }

    std::vector<LoadedMesh> meshes;

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
            v.Normal = { mesh->mNormals[i].x,  mesh->mNormals[i].y,  mesh->mNormals[i].z };
            v.TexCoords = mesh->mTextureCoords[0]
                ? glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y)
                : glm::vec2(0.0f);
            loaded.Vertices.push_back(v);
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

        meshes.push_back(loaded);
    }

    return meshes;
}