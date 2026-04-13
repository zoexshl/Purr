#include "purrpch.h"
#include "RenderCommand.h"
#include <glad/glad.h>

namespace Purr {
    void RenderCommand::DrawIndexed(const std::shared_ptr<VertexArray>& va)
    {
        glDrawElements(GL_TRIANGLES, va->GetIndexBuffer()->GetCount(), GL_UNSIGNED_INT, nullptr);
    }
    void RenderCommand::SetClearColor(float r, float g, float b, float a) { glClearColor(r, g, b, a); }
    void RenderCommand::Clear() { glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); }
    void RenderCommand::EnableDepthTest() { glEnable(GL_DEPTH_TEST); }
}