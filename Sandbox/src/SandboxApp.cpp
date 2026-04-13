#include "purrpch.h"
#include <Purr.h>
#include "imgui/imgui.h"
#include "Purr/Events/MouseEvent.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// -----------------------------------------------------------------------
// Structs
// -----------------------------------------------------------------------
enum class IlluminationModel { Lambert = 0, Phong, BlinnPhong };
static const char* s_ModelNames[] = { "Lambert", "Phong", "Blinn-Phong" };

struct Material
{
    glm::vec3 Ambient = { 0.1f, 0.1f, 0.1f };
    glm::vec3 Diffuse = { 1.0f, 0.4f, 0.7f };
    glm::vec3 Specular = { 1.0f, 1.0f, 1.0f };
    float     Shininess = 32.0f;
    IlluminationModel Model = IlluminationModel::Phong;
};

struct SceneObject
{
    std::string Name;
    glm::vec3   Position = { 0.0f, 0.0f, 0.0f };
    glm::vec3   Rotation = { 0.0f, 0.0f, 0.0f };
    glm::vec3   Scale = { 1.0f, 1.0f, 1.0f };
    Material    Mat;

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
        SceneObject a; a.Name = "Cube 1"; a.Position = { -1.5f, 0.0f, 0.0f };
        a.Mat.Diffuse = { 1.0f, 0.4f, 0.7f }; a.Mat.Model = IlluminationModel::Phong;
        a.Mat.Shininess = 128.0f;   // plus serré = highlight bien visible
        a.Mat.Specular = { 1.0f, 1.0f, 1.0f };
        m_Objects.push_back(a);

        SceneObject b; b.Name = "Cube 2"; b.Position = { 1.5f, 0.0f, 0.0f };
        b.Mat.Diffuse = { 0.4f, 0.7f, 1.0f }; b.Mat.Model = IlluminationModel::BlinnPhong;
        m_Objects.push_back(b);

        SceneObject c; c.Name = "Cube 3"; c.Position = { 0.0f, 0.0f, -2.0f };
        c.Mat.Diffuse = { 0.5f, 1.0f, 0.5f }; c.Mat.Model = IlluminationModel::Lambert;
        m_Objects.push_back(c);
    }

    void OnAttach() override
    {
        Purr::RenderCommand::EnableDepthTest();
    }

    void OnUpdate() override
    {
        ImGuiIO& io = ImGui::GetIO();
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right) && !io.WantCaptureMouse)
            m_Camera.Orbit(io.MouseDelta.x * 0.4f, -io.MouseDelta.y * 0.4f);

        Purr::RenderCommand::SetClearColor(0.12f, 0.12f, 0.12f, 1.0f);
        Purr::RenderCommand::Clear();

        // Position de la caméra (pour specular)
        glm::mat4 view = m_Camera.GetViewMatrix();
        //glm::vec3 camPos = glm::vec3(glm::inverse(view)[3]);

        glm::mat4 invView = glm::inverse(view);
        glm::vec3 camPos = glm::vec3(invView[3][0], invView[3][1], invView[3][2]);

        m_Shader->Bind();
        m_Shader->SetMat4("u_VP", m_Camera.GetViewProjection());
        m_Shader->SetFloat3("u_LightPos", m_LightPos);
        m_Shader->SetFloat3("u_LightColor", m_LightColor);
        m_Shader->SetFloat("u_LightAmbientStrength", m_AmbientStrength);
        m_Shader->SetFloat3("u_CamPos", camPos);

        for (auto& obj : m_Objects)
        {
            glm::mat4 model = obj.GetTransform();
            // Normal matrix = transpose(inverse(model)) — pour les normales non-uniformes
            glm::mat4 normalMat = glm::transpose(glm::inverse(model));

            m_Shader->SetMat4("u_Model", model);
            m_Shader->SetMat4("u_NormalMat", normalMat);
            m_Shader->SetFloat3("u_MatAmbient", obj.Mat.Ambient);
            m_Shader->SetFloat3("u_MatDiffuse", obj.Mat.Diffuse);
            m_Shader->SetFloat3("u_MatSpecular", obj.Mat.Specular);
            m_Shader->SetFloat("u_MatShininess", obj.Mat.Shininess);
            m_Shader->SetInt("u_IllumModel", (int)obj.Mat.Model);

            Purr::RenderCommand::DrawIndexed(m_VA);
        }
    }

    void OnImGuiRender() override
    {
        // ---- Scene --------------------------------------------------------
        ImGui::Begin("Scene");

        bool isPersp = (m_Camera.GetProjectionMode() == Purr::ProjectionMode::Perspective);
        if (ImGui::RadioButton("Perspective", isPersp))
            m_Camera.SetProjectionMode(Purr::ProjectionMode::Perspective);
        ImGui::SameLine();
        if (ImGui::RadioButton("Orthographic", !isPersp))
            m_Camera.SetProjectionMode(Purr::ProjectionMode::Orthographic);

        ImGui::Separator();
        ImGui::Text("Objets (%zu)", m_Objects.size());
        for (int i = 0; i < (int)m_Objects.size(); i++)
        {
            bool sel = (m_Selected == i);
            if (ImGui::Selectable(m_Objects[i].Name.c_str(), sel))
                m_Selected = i;
        }
        ImGui::Separator();
        if (ImGui::Button("+ Ajouter"))
        {
            SceneObject obj;
            obj.Name = "Cube " + std::to_string(m_Objects.size() + 1);
            obj.Mat.Diffuse = { (float)rand() / RAND_MAX, (float)rand() / RAND_MAX, (float)rand() / RAND_MAX };
            m_Objects.push_back(obj);
            m_Selected = (int)m_Objects.size() - 1;
        }
        if (m_Selected >= 0 && m_Selected < (int)m_Objects.size())
        {
            ImGui::SameLine();
            if (ImGui::Button("- Supprimer"))
            {
                m_Objects.erase(m_Objects.begin() + m_Selected);
                m_Selected = glm::max(0, m_Selected - 1);
            }
        }
        ImGui::End();

        // ---- Lumière -------------------------------------------------------
        ImGui::Begin("Lumiere");
        ImGui::DragFloat3("Position", glm::value_ptr(m_LightPos), 0.1f);
        ImGui::ColorEdit3("Couleur", glm::value_ptr(m_LightColor));
        ImGui::SliderFloat("Ambiance", &m_AmbientStrength, 0.0f, 1.0f);
        ImGui::End();

        // ---- Properties ---------------------------------------------------
        if (m_Selected >= 0 && m_Selected < (int)m_Objects.size())
        {
            SceneObject& obj = m_Objects[m_Selected];
            ImGui::Begin("Properties");
            ImGui::Text("%s", obj.Name.c_str());
            ImGui::Separator();

            ImGui::DragFloat3("Position", glm::value_ptr(obj.Position), 0.05f);
            ImGui::DragFloat3("Rotation", glm::value_ptr(obj.Rotation), 0.5f);
            ImGui::DragFloat3("Scale", glm::value_ptr(obj.Scale), 0.01f, 0.01f, 10.0f);

            ImGui::Separator();
            ImGui::Text("Materiau");
            ImGui::ColorEdit3("Diffuse", glm::value_ptr(obj.Mat.Diffuse));
            ImGui::ColorEdit3("Speculaire", glm::value_ptr(obj.Mat.Specular));
            ImGui::SliderFloat("Brillance", &obj.Mat.Shininess, 1.0f, 256.0f);

            ImGui::Separator();
            ImGui::Text("Modele d'illumination");
            int model = (int)obj.Mat.Model;
            if (ImGui::Combo("##illum", &model, s_ModelNames, 3))
                obj.Mat.Model = (IlluminationModel)model;

            ImGui::End();
        }
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
        // position (3) + normale (3) par sommet
        // 24 sommets (4 par face * 6 faces) pour avoir des normales correctes par face
        float verts[] = {
            // Back  (normal 0,0,-1)
            -0.5f,-0.5f,-0.5f, 0,0,-1,   0.5f,-0.5f,-0.5f, 0,0,-1,
             0.5f, 0.5f,-0.5f, 0,0,-1,  -0.5f, 0.5f,-0.5f, 0,0,-1,
             // Front (normal 0,0,1)
             -0.5f,-0.5f, 0.5f, 0,0,1,    0.5f,-0.5f, 0.5f, 0,0,1,
              0.5f, 0.5f, 0.5f, 0,0,1,   -0.5f, 0.5f, 0.5f, 0,0,1,
              // Left  (normal -1,0,0)
              -0.5f,-0.5f,-0.5f,-1,0,0,   -0.5f, 0.5f,-0.5f,-1,0,0,
              -0.5f, 0.5f, 0.5f,-1,0,0,   -0.5f,-0.5f, 0.5f,-1,0,0,
              // Right (normal 1,0,0)
               0.5f,-0.5f,-0.5f, 1,0,0,    0.5f, 0.5f,-0.5f, 1,0,0,
               0.5f, 0.5f, 0.5f, 1,0,0,    0.5f,-0.5f, 0.5f, 1,0,0,
               // Top   (normal 0,1,0)
               -0.5f, 0.5f,-0.5f, 0,1,0,    0.5f, 0.5f,-0.5f, 0,1,0,
                0.5f, 0.5f, 0.5f, 0,1,0,   -0.5f, 0.5f, 0.5f, 0,1,0,
                // Bottom(normal 0,-1,0)
                -0.5f,-0.5f,-0.5f, 0,-1,0,   0.5f,-0.5f,-0.5f, 0,-1,0,
                 0.5f,-0.5f, 0.5f, 0,-1,0,  -0.5f,-0.5f, 0.5f, 0,-1,0,
        };
        uint32_t indices[] = {
             0, 1, 2,  2, 3, 0,   // back
             4, 5, 6,  6, 7, 4,   // front
             8, 9,10, 10,11, 8,   // left
            12,13,14, 14,15,12,   // right
            16,17,18, 18,19,16,   // top
            20,21,22, 22,23,20,   // bottom
        };
        m_VA = std::make_shared<Purr::VertexArray>();
        auto vb = std::make_shared<Purr::VertexBuffer>(verts, sizeof(verts));
        vb->SetLayout({
            { Purr::ShaderDataType::Float3, "a_Position" },
            { Purr::ShaderDataType::Float3, "a_Normal"   },
            });
        m_VA->AddVertexBuffer(vb);
        m_VA->SetIndexBuffer(std::make_shared<Purr::IndexBuffer>(indices, 36));
    }

    void BuildShader()
    {
        std::string vs = R"(
            #version 410 core
            layout(location=0) in vec3 a_Position;
            layout(location=1) in vec3 a_Normal;

            uniform mat4 u_VP;
            uniform mat4 u_Model;
            uniform mat4 u_NormalMat;

            out vec3 v_FragPos;
            out vec3 v_Normal;

            void main() {
                vec4 worldPos   = u_Model * vec4(a_Position, 1.0);
                v_FragPos       = vec3(worldPos);
                v_Normal        = normalize(mat3(u_NormalMat) * a_Normal);
                gl_Position     = u_VP * worldPos;
            }
        )";

        std::string fs = R"(
            #version 410 core

            in vec3 v_FragPos;
            in vec3 v_Normal;

            uniform vec3  u_LightPos;
            uniform vec3  u_LightColor;
            uniform float u_LightAmbientStrength;
            uniform vec3  u_CamPos;

            uniform vec3  u_MatAmbient;
            uniform vec3  u_MatDiffuse;
            uniform vec3  u_MatSpecular;
            uniform float u_MatShininess;

            // 0 = Lambert, 1 = Phong, 2 = Blinn-Phong
            uniform int u_IllumModel;

            out vec4 color;

            void main() {
                vec3 N = normalize(v_Normal);
                vec3 L = normalize(u_LightPos - v_FragPos);
                vec3 V = normalize(u_CamPos   - v_FragPos);

                // Ambiant
                vec3 ambient = u_LightAmbientStrength * u_LightColor * u_MatAmbient;

                // Diffuse (Lambert — commun à tous les modèles)
                float diff   = max(dot(N, L), 0.0);
                vec3 diffuse = diff * u_LightColor * u_MatDiffuse;

                // Speculaire
                vec3 spec = vec3(0.0);
                if (u_IllumModel == 1) {
                    // Phong
                    vec3 R = reflect(-L, N);
                    float s = pow(max(dot(V, R), 0.0), u_MatShininess);
                    spec = s * u_LightColor * u_MatSpecular;
                }
                else if (u_IllumModel == 2) {
                    // Blinn-Phong
                    vec3 H = normalize(L + V);
                    float s = pow(max(dot(N, H), 0.0), u_MatShininess);
                    spec = s * u_LightColor * u_MatSpecular;
                }
                // Lambert : spec reste vec3(0)

                color = vec4(ambient + diffuse + spec, 1.0);
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

    // Lumière ponctuelle
    glm::vec3 m_LightPos = { 3.0f, 3.0f, 3.0f };
    glm::vec3 m_LightColor = { 1.0f, 1.0f, 1.0f };
    float     m_AmbientStrength = 0.15f;
};

class Sandbox : public Purr::Application {
public:
    Sandbox() { PushLayer(new ExampleLayer()); }
    ~Sandbox() {}
};

Purr::Application* Purr::CreateApplication() { return new Sandbox(); }