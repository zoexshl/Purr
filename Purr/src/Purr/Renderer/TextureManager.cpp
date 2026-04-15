#include "purrpch.h"
#include "TextureManager.h"

namespace Purr {

    std::unordered_map<std::string, std::shared_ptr<Texture>> TextureManager::s_Textures;

    std::shared_ptr<Texture> TextureManager::Load(const std::string& path)
    {
        // On vérifie si la texture est déjà chargée
        auto it = s_Textures.find(path);
        if (it != s_Textures.end())
            return it->second;

        // Chargement
        auto texture = std::make_shared<Texture>(path);

        // Si le chargement a échoué (m_RendererID == 0), on ne met pas en cache
        if (texture->GetWidth() == 0 || texture->GetHeight() == 0) {
            PURR_CORE_ERROR("TextureManager: Failed to load texture '{}'", path);
            return nullptr;
        }

        s_Textures[path] = texture;
        PURR_CORE_INFO("Texture loaded: {}", path);

        return texture;
    }

    std::shared_ptr<Texture> TextureManager::Create(uint32_t width, uint32_t height, uint32_t color, const std::string& name)
    {
        std::string key = name.empty() ?
            "__procedural_" + std::to_string(width) + "x" + std::to_string(height) + "_" + std::to_string(color)
            : name;

        auto it = s_Textures.find(key);
        if (it != s_Textures.end())
            return it->second;

        auto texture = std::make_shared<Texture>(width, height, color);
        s_Textures[key] = texture;

        PURR_CORE_INFO("Procedural texture created: {} ({}x{})", key, width, height);

        return texture;
    }

    void TextureManager::Clear()
    {
        s_Textures.clear();
        PURR_CORE_INFO("TextureManager cache cleared");
    }

    void TextureManager::Remove(const std::string& path)
    {
        s_Textures.erase(path);
    }

} 