#include "purrpch.h"
#include "Texture.h"
#include <glad/glad.h>
#include <stb_image.h>

namespace Purr {

    Texture::Texture(const std::string& path)
    {
        stbi_set_flip_vertically_on_load(1);

        int width, height, channels;
        //stbi_set_flip_vertically_on_load(true);
        unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 0);

        if (!data) {
            PURR_CORE_ERROR("Failed to load texture: {0}", path);
            return;
        }

        m_Width = width;
        m_Height = height;

        GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;

        glGenTextures(1, &m_RendererID);
        glBindTexture(GL_TEXTURE_2D, m_RendererID);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        stbi_image_free(data);
    }

    Texture::Texture(uint32_t width, uint32_t height, uint32_t color)
        : m_Width(width), m_Height(height)
    {
        // Génère une texture procédurale (damier)
        std::vector<uint32_t> pixels(width * height);
        for (uint32_t y = 0; y < height; y++)
            for (uint32_t x = 0; x < width; x++)
                pixels[y * width + x] = ((x / 8 + y / 8) % 2 == 0) ? color : 0xFF888888;

        glGenTextures(1, &m_RendererID);
        glBindTexture(GL_TEXTURE_2D, m_RendererID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    }

    Texture::~Texture() { glDeleteTextures(1, &m_RendererID); }

    void Texture::Bind(uint32_t slot) const
    {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, m_RendererID);
    }

    void Texture::Unbind() const { glBindTexture(GL_TEXTURE_2D, 0); }

}