#include "purrpch.h"
#include "OBJLoader.h"

#include <vector>
#include <fstream>
#include <sstream>

#include <glm/glm.hpp>

#include "Purr/Renderer/Buffer.h"
#include "Purr/Renderer/VertexArray.h"


namespace Purr {

    static std::string FindDiffuseTexture(const std::string& objPath)
    {
        // Cherche le .mtl dans le meme dossier
        std::ifstream obj(objPath);
        std::string dir = objPath.substr(0, objPath.find_last_of("/\\") + 1);
        std::string mtlFile;

        std::string line;
        while (std::getline(obj, line)) {
            std::istringstream ss(line);
            std::string token; ss >> token;
            if (token == "mtllib") { ss >> mtlFile; break; }
        }
        if (mtlFile.empty()) return "";

        std::ifstream mtl(dir + mtlFile);
        while (std::getline(mtl, line)) {
            std::istringstream ss(line);
            std::string token; ss >> token;
            if (token == "map_Kd") {
                std::string texName; ss >> texName;
                return dir + texName;
            }
        }
        return "";
    }




    std::shared_ptr<VertexArray> LoadOBJ(const std::string& path, std::string& outTexPath)
    {
        outTexPath = FindDiffuseTexture(path);
        std::ifstream file(path);
        if (!file.is_open()) return nullptr;

        std::vector<glm::vec3> positions, normals;
        std::vector<glm::vec2> texcoords;
        std::vector<ObjVertex> verts;
        std::vector<uint32_t>  indices;

        std::string line;
        while (std::getline(file, line))
        {
            std::istringstream ss(line);
            std::string token;
            ss >> token;

            if (token == "v") {
                glm::vec3 p; ss >> p.x >> p.y >> p.z;
                positions.push_back(p);
            }
            else if (token == "vn") {
                glm::vec3 n; ss >> n.x >> n.y >> n.z;
                normals.push_back(n);
            }
            else if (token == "vt") {
                glm::vec2 t; ss >> t.x >> t.y;
                texcoords.push_back(t);
            }
            else if (token == "f")
            {
                std::vector<std::string> faceTokens;
                std::string ft;
                while (ss >> ft) faceTokens.push_back(ft);

                auto parseVertex = [&](const std::string& t) -> uint32_t {
                    int pi = 0, ti = 0, ni = 0;

                    sscanf_s(t.c_str(), "%d/%d/%d", &pi, &ti, &ni);
                    if (ni == 0) sscanf_s(t.c_str(), "%d//%d", &pi, &ni);
                    if (ti == 0 && ni == 0) sscanf_s(t.c_str(), "%d", &pi);

                    ObjVertex ov{};

                    if (pi > 0 && pi <= (int)positions.size()) {
                        ov.px = positions[pi - 1].x;
                        ov.py = positions[pi - 1].y;
                        ov.pz = positions[pi - 1].z;
                    }
                    if (ni > 0 && ni <= (int)normals.size()) {
                        ov.nx = normals[ni - 1].x;
                        ov.ny = normals[ni - 1].y;
                        ov.nz = normals[ni - 1].z;
                    }
                    if (ti > 0 && ti <= (int)texcoords.size()) {
                        ov.u = texcoords[ti - 1].x;
                        ov.v = texcoords[ti - 1].y;
                    }

                    uint32_t idx = (uint32_t)verts.size();
                    verts.push_back(ov);
                    return idx;
                    };

                uint32_t i0 = parseVertex(faceTokens[0]);
                for (size_t i = 1; i + 1 < faceTokens.size(); i++) {
                    indices.push_back(i0);
                    indices.push_back(parseVertex(faceTokens[i]));
                    indices.push_back(parseVertex(faceTokens[i + 1]));
                }
            }
        }

        if (verts.empty()) return nullptr;

        NormalizeMesh(verts);

        std::vector<float> raw;
        raw.reserve(verts.size() * 8);

        for (auto& ov : verts) {
            raw.insert(raw.end(), {
                ov.px, ov.py, ov.pz,
                ov.nx, ov.ny, ov.nz,
                ov.u,  ov.v
                });
        }

        auto va = std::make_shared<VertexArray>();

        auto vb = std::make_shared<VertexBuffer>(
            raw.data(),
            (uint32_t)(raw.size() * sizeof(float))
        );

        vb->SetLayout({
            { ShaderDataType::Float3, "a_Position" },
            { ShaderDataType::Float3, "a_Normal"   },
            { ShaderDataType::Float2, "a_TexCoord" }
            });

        va->AddVertexBuffer(vb);

        va->SetIndexBuffer(
            std::make_shared<IndexBuffer>(
                indices.data(),
                (uint32_t)indices.size()
            )
        );

        return va;
    }


    void NormalizeMesh(std::vector<ObjVertex>& verts)
    {
        if (verts.empty())
            return;

        glm::vec3 bmin(FLT_MAX);
        glm::vec3 bmax(-FLT_MAX);

        // Calcul du bounding box
        for (auto& ov : verts) {
            glm::vec3 p(ov.px, ov.py, ov.pz);
            bmin = glm::min(bmin, p);
            bmax = glm::max(bmax, p);
        }

        glm::vec3 center = (bmin + bmax) * 0.5f;

        float maxExtent = std::max({ bmax.x - bmin.x, bmax.y - bmin.y, bmax.z - bmin.z });
        float invScale = (maxExtent > 0.0f) ? 1.0f / maxExtent : 1.0f;

        // Normalisation
        for (auto& ov : verts) {
            ov.px = (ov.px - center.x) * invScale;
            ov.py = (ov.py - center.y) * invScale;
            ov.pz = (ov.pz - center.z) * invScale;
        }
    }
}