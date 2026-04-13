#include "purrpch.h"
#include "VertexArray.h"
#include <glad/glad.h>

namespace Purr {

    static GLenum ShaderDataTypeToGL(ShaderDataType t)
    {
        switch (t) {
        case ShaderDataType::Float:
        case ShaderDataType::Float2:
        case ShaderDataType::Float3:
        case ShaderDataType::Float4:
        case ShaderDataType::Mat3:
        case ShaderDataType::Mat4:   return GL_FLOAT;
        case ShaderDataType::Int:    return GL_INT;
        case ShaderDataType::Bool:   return GL_BOOL;
        default: return GL_NONE;
        }
    }

    VertexArray::VertexArray() { glGenVertexArrays(1, &m_RendererID); }
    VertexArray::~VertexArray() { glDeleteVertexArrays(1, &m_RendererID); }
    void VertexArray::Bind()    const { glBindVertexArray(m_RendererID); }
    void VertexArray::Unbind()  const { glBindVertexArray(0); }

    void VertexArray::AddVertexBuffer(const std::shared_ptr<VertexBuffer>& vb)
    {
        PURR_CORE_ASSERT(!vb->GetLayout().GetElements().empty(), "VertexBuffer a pas de layout !");

        glBindVertexArray(m_RendererID);
        vb->Bind();

        uint32_t index = 0;
        for (const auto& element : vb->GetLayout()) {
            glEnableVertexAttribArray(index);
            glVertexAttribPointer(
                index,
                element.GetComponentCount(),
                ShaderDataTypeToGL(element.Type),
                element.Normalized ? GL_TRUE : GL_FALSE,
                vb->GetLayout().GetStride(),
                (const void*)(uintptr_t)element.Offset
            );
            index++;
        }

        m_VertexBuffers.push_back(vb);
    }

    void VertexArray::SetIndexBuffer(const std::shared_ptr<IndexBuffer>& ib)
    {
        glBindVertexArray(m_RendererID);
        ib->Bind();
        m_IndexBuffer = ib;
    }

}