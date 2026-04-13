#pragma once
#include <cstdint>

namespace Purr {

    struct FramebufferSpec {
        uint32_t Width = 1280;
        uint32_t Height = 720;
    };

    class Framebuffer {
    public:
        Framebuffer(const FramebufferSpec& spec);
        ~Framebuffer();

        void Bind();
        void Unbind();
        void Resize(uint32_t width, uint32_t height);

        uint32_t GetColorAttachmentID() const { return m_ColorAttachment; }
        const FramebufferSpec& GetSpec() const { return m_Spec; }

    private:
        void Recreate();

        FramebufferSpec m_Spec;
        uint32_t m_RendererID = 0;
        uint32_t m_ColorAttachment = 0;
        uint32_t m_DepthAttachment = 0;
    };

}