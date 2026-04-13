#include "purrpch.h"
#include <Purr.h>
#include "imgui/imgui.h"
#include "Purr/Events/MouseEvent.h"
#include <glm/gtc/matrix_transform.hpp>

class ExampleLayer : public Purr::Layer {
public:
    ExampleLayer()
        : Layer("Example"),
        m_Camera(60.0f, 1280.0f / 720.0f)
    {
        // ---- Cube ----
        // 8 sommets uniques : position + couleur
        float verts[] = {
            -0.5f, -0.5f, -0.5f,  1.0f, 0.4f, 0.7f,
             0.5f, -0.5f, -0.5f,  0.8f, 0.2f, 0.9f,
             0.5f,  0.5f, -0.5f,  1.0f, 0.7f, 0.9f,
            -0.5f,  0.5f, -0.5f,  0.6f, 0.3f, 0.8f,
            -0.5f, -0.5f,  0.5f,  1.0f, 0.5f, 0.8f,
             0.5f, -0.5f,  0.5f,  0.9f, 0.3f, 1.0f,
             0.5f,  0.5f,  0.5f,  1.0f, 0.8f, 1.0f,
            -0.5f,  0.5f,  0.5f,  0.7f, 0.4f, 0.9f,
        };
        uint32_t indices[] = {
            0,1,2, 2,3,0,   // back
            4,5,6, 6,7,4,   // front
            0,4,7, 7,3,0,   // left
            1,5,6, 6,2,1,   // right
            3,2,6, 6,7,3,   // top
            0,1,5, 5,4,0,   // bottom
        };

        m_VA = std::make_shared<Purr::VertexArray>();
        auto vb = std::make_shared<Purr::VertexBuffer>(verts, sizeof(verts));
        vb->SetLayout({
            { Purr::ShaderDataType::Float3, "a_Position" },
            { Purr::ShaderDataType::Float3, "a_Color"    },
            });
        m_VA->AddVertexBuffer(vb);
        m_VA->SetIndexBuffer(std::make_shared<Purr::IndexBuffer>(indices, 36));

        // ---- Shader ----
        std::string vs = R"(
            #version 410 core
            layout(location=0) in vec3 a_Position;
            layout(location=1) in vec3 a_Color;
            uniform mat4 u_VP;
            uniform mat4 u_Model;
            out vec3 v_Color;
            void main() {
                v_Color = a_Color;
                gl_Position = u_VP * u_Model * vec4(a_Position, 1.0);
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
    void OnAttach() override
    {
        Purr::RenderCommand::EnableDepthTest();
    }

    void OnUpdate() override
    {
        // Orbite : clic droit + drag
        ImGuiIO& io = ImGui::GetIO();
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right) && !io.WantCaptureMouse)
        {
            m_Camera.Orbit(io.MouseDelta.x * 0.4f, -io.MouseDelta.y * 0.4f);
        }

        Purr::RenderCommand::SetClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        Purr::RenderCommand::Clear();

        glm::mat4 model = glm::rotate(glm::mat4(1.0f),
            glm::radians(m_Rotation), glm::vec3(0.0f, 1.0f, 0.0f));

        m_Shader->Bind();
        m_Shader->SetMat4("u_VP", m_Camera.GetViewProjection());
        m_Shader->SetMat4("u_Model", model);

        Purr::RenderCommand::DrawIndexed(m_VA);
    }

    void OnImGuiRender() override
    {
        ImGui::Begin("Purr Engine");

        // Toggle projection
        bool isPerspective = (m_Camera.GetProjectionMode() == Purr::ProjectionMode::Perspective);
        if (ImGui::RadioButton("Perspective", isPerspective))
            m_Camera.SetProjectionMode(Purr::ProjectionMode::Perspective);
        ImGui::SameLine();
        if (ImGui::RadioButton("Orthographic", !isPerspective))
            m_Camera.SetProjectionMode(Purr::ProjectionMode::Orthographic);

        ImGui::Separator();
        ImGui::SliderFloat("Rotation Y", &m_Rotation, 0.0f, 360.0f);
        ImGui::Text("Clic droit + drag : orbite");
        ImGui::Text("Scroll : zoom");

        ImGui::End();
    }

    void OnEvent(Purr::Event& e) override
    {
        Purr::EventDispatcher dispatcher(e);
        dispatcher.Dispatch<Purr::MouseScrolledEvent>([this](Purr::MouseScrolledEvent& e) {
            m_Camera.Zoom(e.GetYOffset() * 0.3f);
            return false;
            });
    }

private:
    std::shared_ptr<Purr::VertexArray> m_VA;
    std::shared_ptr<Purr::Shader>      m_Shader;
    Purr::Camera                       m_Camera;
    float                              m_Rotation = 0.0f;
};

class Sandbox : public Purr::Application {
public:
    Sandbox() { PushLayer(new ExampleLayer()); }
    ~Sandbox() {}
};

Purr::Application* Purr::CreateApplication() { return new Sandbox(); }