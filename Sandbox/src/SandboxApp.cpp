#include "purrpch.h"
#include <Purr.h>
#include "imgui/imgui.h"


class ExampleLayer : public Purr::Layer {
public:
    ExampleLayer() : Layer("Example")
    {
        // --- Vertex Array ---
        m_VA = std::make_shared<Purr::VertexArray>();

        float verts[] = {
            // position          // color
            -0.5f, -0.5f, 0.0f,  1.0f, 0.4f, 0.7f,
             0.5f, -0.5f, 0.0f,  0.8f, 0.2f, 0.9f,
             0.0f,  0.5f, 0.0f,  1.0f, 0.7f, 0.9f,
        };
        auto vb = std::make_shared<Purr::VertexBuffer>(verts, sizeof(verts));
        vb->SetLayout({
            { Purr::ShaderDataType::Float3, "a_Position" },
            { Purr::ShaderDataType::Float3, "a_Color"    },
            });
        m_VA->AddVertexBuffer(vb);

        uint32_t indices[] = { 0, 1, 2 };
        m_VA->SetIndexBuffer(std::make_shared<Purr::IndexBuffer>(indices, 3));

        // --- Shader ---
        std::string vs = R"(
            #version 410 core
            layout(location=0) in vec3 a_Position;
            layout(location=1) in vec3 a_Color;
            out vec3 v_Color;
            void main() {
                v_Color = a_Color;
                gl_Position = vec4(a_Position, 1.0);
            }
        )";
        std::string fs = R"(
            #version 410 core
            in  vec3 v_Color;
            out vec4 color;
            void main() {
                color = vec4(v_Color, 1.0);
            }
        )";
        m_Shader = std::make_shared<Purr::Shader>(vs, fs);
    }

    void OnUpdate() override
    {
        m_Shader->Bind();
        m_VA->Bind();
        Purr::RenderCommand::DrawIndexed(m_VA);
    }

    virtual void OnImGuiRender() override
    {
        ImGui::Begin("Purr Engine");
        ImGui::Text("Triangle test!");
        ImGui::End();
    }

    void OnEvent(Purr::Event& event) override {}

private:
    std::shared_ptr<Purr::VertexArray>  m_VA;
    std::shared_ptr<Purr::Shader>       m_Shader;
};

class Sandbox : public Purr::Application {
public:
    Sandbox() { PushLayer(new ExampleLayer()); }
    ~Sandbox() {}
};

Purr::Application* Purr::CreateApplication() { return new Sandbox(); }