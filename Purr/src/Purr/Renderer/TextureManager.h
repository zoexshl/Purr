#pragma once
#include "Texture.h"
#include <unordered_map>
#include <string>
#include <memory>

namespace Purr {

    class TextureManager {
    public:
        // Charge ou récupère une texture depuis un fichier (cache automatique)
        static std::shared_ptr<Texture> Load(const std::string& path);

        // Crée une texture unie / procédurale et la met en cache (optionnel)
        static std::shared_ptr<Texture> Create(uint32_t width, uint32_t height, uint32_t color, const std::string& name = "");

        // Vide le cache (utile au shutdown ou pour reload)
        static void Clear();

        // Optionnel : supprimer une texture spécifique du cache
        static void Remove(const std::string& path);

    private:
        static std::unordered_map<std::string, std::shared_ptr<Texture>> s_Textures;
    };

} // namespace Purr