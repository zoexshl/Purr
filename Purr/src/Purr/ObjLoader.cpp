#include "purrpch.h"
#include "OBJLoader.h"

#include <vector>
#include <fstream>
#include <sstream>

#include <glm/glm.hpp>

#include "Purr/Renderer/Buffer.h"
#include "Purr/Renderer/VertexArray.h"

#include <filesystem>
#include <cctype>

namespace Purr {
    struct MtlMaterialInfo {
        std::string TexturePath;
        glm::vec3 DiffuseTint = glm::vec3(1.0f);
    };

    struct ObjBuildSubmesh {
        std::string MaterialName;
        std::string TexturePath;
        glm::vec3 DiffuseTint = glm::vec3(1.0f);
        std::vector<ObjVertex> Vertices;
        std::vector<uint32_t> Indices;
    };

    static std::string Trim(const std::string& input)
    {
        size_t start = 0;
        while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start])))
            ++start;
        size_t end = input.size();
        while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1])))
            --end;
        return input.substr(start, end - start);
    }

    static std::string ResolvePath(const std::string& baseDir, const std::string& relativePath)
    {
        std::filesystem::path resolved = std::filesystem::path(baseDir) / std::filesystem::path(relativePath);
        return resolved.lexically_normal().string();
    }

    // Supporte "map_Kd" avec options (ex: map_Kd -s 1 1 1 tex.png)
    static std::string ParseMapKdPath(const std::string& line)
    {
        std::istringstream ss(line);
        std::string token;
        ss >> token; // map_Kd

        std::string lastPathToken;
        while (ss >> token) {
            if (token.empty())
                continue;

            if (token[0] == '-') {
                // Sauter les valeurs de l'option (si présentes)
                std::string maybeValue;
                while (ss.peek() == ' ')
                    ss.get();
                continue;
            }

            lastPathToken = token;
        }

        // Si le parser tokenisé n'a rien trouvé, fallback sur la fin brute de ligne
        if (lastPathToken.empty()) {
            size_t pos = line.find("map_Kd");
            if (pos != std::string::npos) {
                std::string raw = Trim(line.substr(pos + 6));
                if (!raw.empty())
                    return raw;
            }
        }

        return lastPathToken;
    }

    static std::string FindDiffuseTexture(const std::string& objPath)
    {
        // Cherche le .mtl dans le meme dossier
        std::ifstream obj(objPath);
        if (!obj.is_open())
            return "";

        std::string dir = objPath.substr(0, objPath.find_last_of("/\\") + 1);
        std::string mtlFile;

        std::string line;
        while (std::getline(obj, line)) {
            std::istringstream ss(line);
            std::string token; ss >> token;
            if (token == "mtllib") { ss >> mtlFile; break; }
        }
        if (mtlFile.empty()) return "";

        std::ifstream mtl(ResolvePath(dir, mtlFile));
        if (!mtl.is_open())
            return "";

        while (std::getline(mtl, line)) {
            std::string trimmed = Trim(line);
            if (trimmed.rfind("map_Kd", 0) == 0) {
                std::string texName = ParseMapKdPath(trimmed);
                if (!texName.empty())
                    return ResolvePath(dir, texName);
            }
        }
        return "";
    }

    static std::unordered_map<std::string, MtlMaterialInfo> LoadMtlMaterialInfos(const std::string& objPath)
    {
        std::unordered_map<std::string, MtlMaterialInfo> maps;
        std::ifstream obj(objPath);
        if (!obj.is_open())
            return maps;

        std::string dir = objPath.substr(0, objPath.find_last_of("/\\") + 1);
        std::string mtlFile;
        std::string line;
        while (std::getline(obj, line)) {
            std::istringstream ss(line);
            std::string token;
            ss >> token;
            if (token == "mtllib") {
                ss >> mtlFile;
                break;
            }
        }
        if (mtlFile.empty())
            return maps;

        std::ifstream mtl(ResolvePath(dir, mtlFile));
        if (!mtl.is_open())
            return maps;

        std::string currentMaterial;
        while (std::getline(mtl, line)) {
            std::string trimmed = Trim(line);
            if (trimmed.empty() || trimmed[0] == '#')
                continue;

            if (trimmed.rfind("newmtl", 0) == 0) {
                std::istringstream ss(trimmed);
                std::string token;
                ss >> token;
                ss >> currentMaterial;
                if (!currentMaterial.empty())
                    maps[currentMaterial] = MtlMaterialInfo{};
                continue;
            }

            if (trimmed.rfind("map_Kd", 0) == 0 && !currentMaterial.empty()) {
                std::string texName = ParseMapKdPath(trimmed);
                if (!texName.empty())
                    maps[currentMaterial].TexturePath = ResolvePath(dir, texName);
                continue;
            }

            // Certains exports Roblox ont seulement map_Ka.
            if (trimmed.rfind("map_Ka", 0) == 0 && !currentMaterial.empty()) {
                if (!maps[currentMaterial].TexturePath.empty())
                    continue; // map_Kd déjà trouvée, on garde la priorité au diffuse.
                std::string texName = ParseMapKdPath(trimmed);
                if (!texName.empty())
                    maps[currentMaterial].TexturePath = ResolvePath(dir, texName);
                continue;
            }

            if (trimmed.rfind("Kd", 0) == 0 && !currentMaterial.empty()) {
                std::istringstream ss(trimmed);
                std::string token;
                float r = 1.0f, g = 1.0f, b = 1.0f;
                ss >> token >> r >> g >> b;
                maps[currentMaterial].DiffuseTint = glm::vec3(r, g, b);
            }
        }

        return maps;
    }

    static void NormalizeBuildSubmeshes(std::vector<ObjBuildSubmesh>& submeshes)
    {
        glm::vec3 bmin(FLT_MAX);
        glm::vec3 bmax(-FLT_MAX);
        bool hasAnyVertex = false;

        for (auto& sm : submeshes) {
            for (auto& ov : sm.Vertices) {
                hasAnyVertex = true;
                glm::vec3 p(ov.px, ov.py, ov.pz);
                bmin = glm::min(bmin, p);
                bmax = glm::max(bmax, p);
            }
        }

        if (!hasAnyVertex)
            return;

        glm::vec3 center = (bmin + bmax) * 0.5f;
        float maxExtent = std::max({ bmax.x - bmin.x, bmax.y - bmin.y, bmax.z - bmin.z });
        float invScale = (maxExtent > 0.0f) ? 1.0f / maxExtent : 1.0f;

        for (auto& sm : submeshes) {
            for (auto& ov : sm.Vertices) {
                ov.px = (ov.px - center.x) * invScale;
                ov.py = (ov.py - center.y) * invScale;
                ov.pz = (ov.pz - center.z) * invScale;
            }
        }
    }

    bool LoadOBJMulti(const std::string& path, std::vector<ObjSubmesh>& outSubmeshes, std::string& outFirstTexPath)
    {
        outSubmeshes.clear();
        outFirstTexPath.clear();

        std::ifstream file(path);
        if (!file.is_open())
            return false;

        std::vector<glm::vec3> positions, normals;
        std::vector<glm::vec2> texcoords;
        std::unordered_map<std::string, MtlMaterialInfo> materialInfos = LoadMtlMaterialInfos(path);

        std::vector<ObjBuildSubmesh> buildSubmeshes;
        std::unordered_map<std::string, size_t> submeshByMaterial;

        auto getSubmesh = [&](const std::string& materialName) -> ObjBuildSubmesh& {
            auto it = submeshByMaterial.find(materialName);
            if (it != submeshByMaterial.end())
                return buildSubmeshes[it->second];

            ObjBuildSubmesh sm;
            sm.MaterialName = materialName;
            auto matIt = materialInfos.find(materialName);
            if (matIt != materialInfos.end()) {
                sm.TexturePath = matIt->second.TexturePath;
                sm.DiffuseTint = matIt->second.DiffuseTint;
            }
            buildSubmeshes.push_back(sm);
            submeshByMaterial[materialName] = buildSubmeshes.size() - 1;
            return buildSubmeshes.back();
        };

        std::string currentMaterial = "__default__";
        getSubmesh(currentMaterial);

        std::string line;
        while (std::getline(file, line))
        {
            std::istringstream ss(line);
            std::string token;
            ss >> token;
            if (token.empty() || token[0] == '#')
                continue;

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
            else if (token == "usemtl") {
                std::string matName;
                ss >> matName;
                if (!matName.empty()) {
                    currentMaterial = matName;
                    getSubmesh(currentMaterial);
                }
            }
            else if (token == "f")
            {
                std::vector<std::string> faceTokens;
                std::string ft;
                while (ss >> ft) faceTokens.push_back(ft);
                if (faceTokens.size() < 3)
                    continue;

                ObjBuildSubmesh& target = getSubmesh(currentMaterial);
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

                    uint32_t idx = (uint32_t)target.Vertices.size();
                    target.Vertices.push_back(ov);
                    return idx;
                };

                uint32_t i0 = parseVertex(faceTokens[0]);
                for (size_t i = 1; i + 1 < faceTokens.size(); i++) {
                    target.Indices.push_back(i0);
                    target.Indices.push_back(parseVertex(faceTokens[i]));
                    target.Indices.push_back(parseVertex(faceTokens[i + 1]));
                }
            }
        }

        NormalizeBuildSubmeshes(buildSubmeshes);

        for (auto& sm : buildSubmeshes)
        {
            if (sm.Vertices.empty() || sm.Indices.empty())
                continue;

            std::vector<float> raw;
            raw.reserve(sm.Vertices.size() * 8);
            for (auto& ov : sm.Vertices) {
                raw.insert(raw.end(), { ov.px, ov.py, ov.pz, ov.nx, ov.ny, ov.nz, ov.u, ov.v });
            }

            auto va = std::make_shared<VertexArray>();
            auto vb = std::make_shared<VertexBuffer>(raw.data(), (uint32_t)(raw.size() * sizeof(float)));
            vb->SetLayout({
                { ShaderDataType::Float3, "a_Position" },
                { ShaderDataType::Float3, "a_Normal"   },
                { ShaderDataType::Float2, "a_TexCoord" }
            });
            va->AddVertexBuffer(vb);
            va->SetIndexBuffer(std::make_shared<IndexBuffer>(sm.Indices.data(), (uint32_t)sm.Indices.size()));

            ObjSubmesh out{};
            out.Mesh = va;
            out.TexturePath = sm.TexturePath;
            out.MaterialName = sm.MaterialName;
            out.DiffuseTint = sm.DiffuseTint;
            outSubmeshes.push_back(out);

            if (outFirstTexPath.empty() && !sm.TexturePath.empty())
                outFirstTexPath = sm.TexturePath;
        }

        return !outSubmeshes.empty();
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