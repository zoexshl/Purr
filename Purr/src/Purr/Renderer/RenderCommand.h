#pragma once
#include "VertexArray.h"

namespace Purr {
    class RenderCommand {
    public:
        static void DrawLines(const std::shared_ptr<VertexArray>& va);
        static void DrawWireframe(const std::shared_ptr<VertexArray>& va);
        static void EnableDepthTest();
        static void DrawIndexed(const std::shared_ptr<VertexArray>& va);
        static void SetClearColor(float r, float g, float b, float a);
        static void Clear();
    };
}