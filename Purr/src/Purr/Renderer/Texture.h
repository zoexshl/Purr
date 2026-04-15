#pragma once
#include <string>

namespace Purr {

    class Texture {
    public:
        Texture(const std::string& path);
        Texture(uint32_t width, uint32_t height, uint32_t color); // couleur unie RGBA
        ~Texture();

        void Bind(uint32_t slot = 0) const;
        void Unbind() const;

        uint32_t GetWidth()  const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }

        // In case echec chagement
        uint32_t GetRendererID() const { return m_RendererID; }


        bool IsValid() const { return m_RendererID != 0; }

    private:
        uint32_t m_RendererID = 0;
        uint32_t m_Width = 0, m_Height = 0;
    };

}