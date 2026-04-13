#include "purrpch.h"
#include <Purr.h>
#include "imgui/imgui.h"
#include "Purr/Events/MouseEvent.h"
#include <glm/gtc/matrix_transform.hpp>

// -----------------------------------------------------------------------
// SceneObject : un noeud du graphe de scène
// -----------------------------------------------------------------------
struct SceneObject
{
    std::string Name;
    glm::vec3   Position = { 0.0f, 0.0f, 0.0f };
    glm::vec3   Rotation = { 0.0f, 0.0f, 0.0f }; // degrés
    glm::vec3   Scale = { 1.0f, 1.0f, 1.0f };
    glm::vec3   Color = { 1.0f, 0.4f, 0.7f };

    glm::mat4 GetTransform() const
    {
        glm::mat4 t = glm::translate(glm::mat4(1.0f), Position);
        t = glm::rotate(t, glm::radians(Rotation.x), { 1,0,0 });
        t = glm::rotate(t, glm::radians(Rotation.y), { 0,1,0 });
        t = glm::rotate(t, glm::radians(Rotation.z), { 0,0,1 });
        t = glm::scale(t, Scale);
        return t;
    }
};

// -----------------------------------------------------------------------
// ExampleLayer
// -----------------------------------------------------------------------
class ExampleLayer : public Purr::Layer {
public:
    ExampleLayer()
        : Layer("Example"),
        m_Camera(60.0f, 1280.0f / 720.0f)
    {
        BuildCubeMesh();
        BuildShader();

        // Objets de départ
        m_Objects.push_back({ "Cube 1", {  0.0f, 0.0f, 0.0f } });
        m_Objects.push_back({ "Cube 2", {  2.0f, 0.0f, 0.0f }, {0,45,0}, {0.8f,0.8f,0.8f}, {0.5f,0.8f,1.0f} });
    }

    void OnAttach() override
    {
        Purr::RenderCommand::EnableDepthTest();
    }

    void OnUpdate() override
    {
        // Orbite clic droit
        ImGuiIO& io = ImGui::GetIO();
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right) && !io.WantCaptureMouse)
            m_Camera.Orbit(io.MouseDelta.x * 0.4f, -io.MouseDelta.y * 0.4f);

        Purr::RenderCommand::SetClearColor(0.12f, 0.12f, 0.12f, 1.0f);
        Purr::RenderCommand::Clear();

        m_Shader->Bind();
        m_Shader->SetMat4("u_VP", m_Camera.GetViewProjection());

        for (auto& obj : m_Objects)
        {
            m_Shader->SetMat4("u_Model", obj.GetTransform());
            m_Shader->SetFloat3("u_Color", obj.Color);
            Purr::RenderCommand::DrawIndexed(m_VA);
        }
    }

    void OnImGuiRender() override
    {
        // ---- Panel : Scene ------------------------------------------------
        ImGui::Begin("Scene");

        // Projection toggle
        bool isPersp = (m_Camera.GetProjectionMode() == Purr::ProjectionMode::Perspective);
        if (ImGui::RadioButton("Perspective", isPersp))  m_Camera.SetProjectionMode(Purr::ProjectionMode::Perspective);
        ImGui::SameLine();
        if (ImGui::RadioButton("Orthographic", !isPersp)) m_Camera.SetProjectionMode(Purr::ProjectionMode::Orthographic);

        ImGui::Separator();
        ImGui::Text("Objets (%zu)", m_Objects.size());
        ImGui::Separator();

        // Liste des objets
        for (int i = 0; i < (int)m_Objects.size(); i++)
        {
            bool selected = (m_Selected == i);
            if (ImGui::Selectable(m_Objects[i].Name.c_str(), selected))
                m_Selected = i;
        }

        ImGui::Separator();

        // Ajouter un objet
        if (ImGui::Button("+ Ajouter cube"))
        {
            SceneObject obj;
            obj.Name = "Cube " + std::to_string(m_Objects.size() + 1);
            obj.Color = { (float)rand() / RAND_MAX, (float)rand() / RAND_MAX, (float)rand() / RAND_MAX };
            m_Objects.push_back(obj);
            m_Selected = (int)m_Objects.size() - 1;
        }

        // Supprimer l'objet sélectionné
        if (m_Selected >= 0 && m_Selected < (int)m_Objects.size())
        {
            ImGui::SameLine();
            if (ImGui::Button("- Supprimer"))
            {
                m_Objects.erase(m_Objects.begin() + m_Selected);
                m_Selected = (int)m_Objects.size() - 1;
            }
        }

        ImGui::End();

        // ---- Panel : Properties -------------------------------------------
        if (m_Selected >= 0 && m_Selected < (int)m_Objects.size())
        {
            SceneObject& obj = m_Objects[m_Selected];

            ImGui::Begin("Properties");
            ImGui::Text("%s", obj.Name.c_str());
            ImGui::Separator();

            ImGui::DragFloat3("Position", &obj.Position.x, 0.05f);
            ImGui::DragFloat3("Rotation", &obj.Rotation.x, 0.5f);
            ImGui::DragFloat3("Scale", &obj.Scale.x, 0.01f, 0.01f, 10.0f);

            ImGui::Separator();
            ImGui::ColorEdit3("Couleur", &obj.Color.x);

            ImGui::End();
        }

        // ---- Panel : Camera -----------------------------------------------
        ImGui::Begin("Camera");
        ImGui::Text("Clic droit + drag : orbite");
        ImGui::Text("Scroll : zoom");
        float radius = m_Camera.GetRadius();
        ImGui::Text("Rayon: %.2f", radius);
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
    void BuildCubeMesh()
    {
        float verts[] = {
            -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f, 0.5f,-0.5f, -0.5f, 0.5f,-0.5f,
            -0.5f,-0.5f, 0.5f,  0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f,
        };
        uint32_t indices[] = {
            0,1,2, 2,3,0,  4,5,6, 6,7,4,
            0,4,7, 7,3,0,  1,5,6, 6,2,1,
            3,2,6, 6,7,3,  0,1,5, 5,4,0,
        };
        m_VA = std::make_shared<Purr::VertexArray>();
        auto vb = std::make_shared<Purr::VertexBuffer>(verts, sizeof(verts));
        vb->SetLayout({ { Purr::ShaderDataType::Float3, "a_Position" } });
        m_VA->AddVertexBuffer(vb);
        m_VA->SetIndexBuffer(std::make_shared<Purr::IndexBuffer>(indices, 36));
    }

    void BuildShader()
    {
        std::string vs = R"(
            #version 410 core
            layout(location=0) in vec3 a_Position;
            uniform mat4 u_VP;
            uniform mat4 u_Model;
            void main() {
                gl_Position = u_VP * u_Model * vec4(a_Position, 1.0);
            }
        )";
        std::string fs = R"(
            #version 410 core
            uniform vec3 u_Color;
            out vec4 color;
            void main() {
                color = vec4(u_Color, 1.0);
            }
        )";
        m_Shader = std::make_shared<Purr::Shader>(vs, fs);
    }

private:
    std::shared_ptr<Purr::VertexArray> m_VA;
    std::shared_ptr<Purr::Shader>      m_Shader;
    Purr::Camera                       m_Camera;

    std::vector<SceneObject> m_Objects;
    int                      m_Selected = 0;
};

class Sandbox : public Purr::Application {
public:
    Sandbox() { PushLayer(new ExampleLayer()); }
    ~Sandbox() {}
};

Purr::Application* Purr::CreateApplication() { return new Sandbox(); }