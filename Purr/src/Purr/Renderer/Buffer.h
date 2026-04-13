#pragma once
#include <vector>
#include <string>

namespace Purr {

    // -----------------------------------------------------------------------
    // BufferLayout
    // -----------------------------------------------------------------------
    enum class ShaderDataType { None = 0, Float, Float2, Float3, Float4, Mat3, Mat4, Int, Bool };

    static uint32_t ShaderDataTypeSize(ShaderDataType t)
    {
        switch (t) {
        case ShaderDataType::Float:  return 4;
        case ShaderDataType::Float2: return 8;
        case ShaderDataType::Float3: return 12;
        case ShaderDataType::Float4: return 16;
        case ShaderDataType::Mat3:   return 36;
        case ShaderDataType::Mat4:   return 64;
        case ShaderDataType::Int:    return 4;
        case ShaderDataType::Bool:   return 1;
        default: return 0;
        }
    }

    struct BufferElement {
        std::string    Name;
        ShaderDataType Type;
        uint32_t       Size;
        uint32_t       Offset = 0;
        bool           Normalized = false;

        BufferElement(ShaderDataType type, const std::string& name, bool normalized = false)
            : Name(name), Type(type), Size(ShaderDataTypeSize(type)), Normalized(normalized) {
        }

        uint32_t GetComponentCount() const {
            switch (Type) {
            case ShaderDataType::Float:  return 1;
            case ShaderDataType::Float2: return 2;
            case ShaderDataType::Float3: return 3;
            case ShaderDataType::Float4: return 4;
            case ShaderDataType::Mat3:   return 9;
            case ShaderDataType::Mat4:   return 16;
            case ShaderDataType::Int:    return 1;
            case ShaderDataType::Bool:   return 1;
            default: return 0;
            }
        }
    };

    class BufferLayout {
    public:
        BufferLayout() = default;
        BufferLayout(std::initializer_list<BufferElement> elements) : m_Elements(elements)
        {
            uint32_t offset = 0;
            m_Stride = 0;
            for (auto& e : m_Elements) {
                e.Offset = offset;
                offset += e.Size;
                m_Stride += e.Size;
            }
        }

        uint32_t GetStride() const { return m_Stride; }
        const std::vector<BufferElement>& GetElements() const { return m_Elements; }

        std::vector<BufferElement>::iterator begin() { return m_Elements.begin(); }
        std::vector<BufferElement>::iterator end() { return m_Elements.end(); }
        std::vector<BufferElement>::const_iterator begin() const { return m_Elements.begin(); }
        std::vector<BufferElement>::const_iterator end()   const { return m_Elements.end(); }

    private:
        std::vector<BufferElement> m_Elements;
        uint32_t m_Stride = 0;
    };

    // -----------------------------------------------------------------------
    // VertexBuffer
    // -----------------------------------------------------------------------
    class VertexBuffer {
    public:
        VertexBuffer(float* vertices, uint32_t size);
        ~VertexBuffer();

        void Bind()   const;
        void Unbind() const;

        void SetLayout(const BufferLayout& layout) { m_Layout = layout; }
        const BufferLayout& GetLayout() const { return m_Layout; }

    private:
        uint32_t     m_RendererID;
        BufferLayout m_Layout;
    };

    // -----------------------------------------------------------------------
    // IndexBuffer
    // -----------------------------------------------------------------------
    class IndexBuffer {
    public:
        IndexBuffer(uint32_t* indices, uint32_t count);
        ~IndexBuffer();

        void Bind()   const;
        void Unbind() const;

        uint32_t GetCount() const { return m_Count; }

    private:
        uint32_t m_RendererID;
        uint32_t m_Count;
    };

}