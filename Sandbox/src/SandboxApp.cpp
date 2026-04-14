#include "purrpch.h"
#include <Purr.h>
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "Purr/Events/MouseEvent.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <Windows.h>
#include <commdlg.h>

// -----------------------------------------------------------------------
// File dialog
// -----------------------------------------------------------------------
static std::string OpenFileDialog()
{
    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = "Images\0*.png;*.jpg;*.jpeg;*.bmp;*.tga\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Choisir une texture";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn)) return std::string(filename);
    return "";
}

// -----------------------------------------------------------------------
// Structs
// -----------------------------------------------------------------------
enum class IlluminationModel { Lambert = 0, Phong, BlinnPhong };
static const char* s_ModelNames[] = { "Lambert", "Phong", "Blinn-Phong" };

struct Light {
    glm::vec3 Position = { 0.0f, 3.0f, 0.0f };
    glm::vec3 Color = { 1.0f, 1.0f, 1.0f };
    float Intensity = 1.0f, Constant = 1.0f, Linear = 0.09f, Quadratic = 0.032f;
    bool Enabled = true;
};

struct Material {
    glm::vec3 Ambient = { 0.1f,0.1f,0.1f }, Diffuse = { 1.0f,1.0f,1.0f }, Specular = { 1.0f,1.0f,1.0f };
    float Shininess = 64.0f;
    IlluminationModel Model = IlluminationModel::Phong;
};

struct SceneObject {
    std::string Name;
    glm::vec3 Position = { 0,0,0 }, Rotation = { 0,0,0 }, Scale = { 1,1,1 };
    Material Mat;
    bool IsPlane = false;
    std::shared_ptr<Purr::Texture> Tex = nullptr;
    std::string TexPath = "";

    glm::mat4 GetTransform() const {
        glm::mat4 t = glm::translate(glm::mat4(1.0f), Position);
        t = glm::rotate(t, glm::radians(Rotation.x), { 1,0,0 });
        t = glm::rotate(t, glm::radians(Rotation.y), { 0,1,0 });
        t = glm::rotate(t, glm::radians(Rotation.z), { 0,0,1 });
        return glm::scale(t, Scale);
    }
};

// -----------------------------------------------------------------------
// Gizmo axis enum
// -----------------------------------------------------------------------
enum class GizmoAxis { None, X, Y, Z };

// -----------------------------------------------------------------------
// ExampleLayer
// -----------------------------------------------------------------------
class ExampleLayer : public Purr::Layer {
public:
    ExampleLayer() : Layer("Example"), m_Camera(60.0f, 1280.0f / 720.0f)
    {
        BuildCubeMesh(); BuildPlaneMesh();
        BuildShader(); BuildTexturedShader();
        BuildWireShader(); BuildBBoxMesh();
        BuildGizmoShader(); BuildArrowMesh();

        m_CheckerTex = std::make_shared<Purr::Texture>(128, 128, 0xFFB97FFF);

        // Framebuffer
        Purr::FramebufferSpec spec;
        spec.Width = 1280; spec.Height = 720;
        m_Framebuffer = std::make_shared<Purr::Framebuffer>(spec);

        SceneObject a; a.Name = "Cube 1"; a.Position = { -1.5f,0,0 };
        a.Mat.Diffuse = { 1.0f,0.4f,0.7f }; a.Mat.Model = IlluminationModel::Phong;
        m_Objects.push_back(a);

        SceneObject b; b.Name = "Cube 2"; b.Position = { 1.5f,0,0 };
        b.Mat.Diffuse = { 0.4f,0.7f,1.0f }; b.Mat.Model = IlluminationModel::BlinnPhong;
        m_Objects.push_back(b);

        SceneObject c; c.Name = "Cube 3"; c.Position = { 0,0,-2.5f };
        c.Mat.Diffuse = { 0.5f,1.0f,0.5f }; c.Mat.Model = IlluminationModel::Lambert;
        m_Objects.push_back(c);

        SceneObject plane; plane.Name = "Plan (sol)"; plane.IsPlane = true;
        plane.Position = { 0,-0.5f,0 }; plane.Scale = { 6,1,6 };
        plane.Mat.Model = IlluminationModel::Lambert;
        plane.Tex = m_CheckerTex;
        m_Objects.push_back(plane);

        m_Lights[0].Position = { 3.0f, 3.0f,  3.0f }; m_Lights[0].Color = { 1.0f,1.0f,1.0f };
        m_Lights[1].Position = { -3.0f, 2.0f,  2.0f }; m_Lights[1].Color = { 1.0f,0.4f,0.4f };
        m_Lights[2].Position = { 0.0f, 4.0f, -3.0f }; m_Lights[2].Color = { 0.4f,0.6f,1.0f };
        m_Lights[3].Position = { 0.0f,-2.0f,  0.0f }; m_Lights[3].Color = { 0.8f,1.0f,0.4f };
        m_Lights[3].Enabled = false;
    }

    void OnAttach() override { Purr::RenderCommand::EnableDepthTest(); }

    void OnUpdate() override
    {
        // Resize framebuffer si viewport a change
        if (m_ViewportSize.x > 0 && m_ViewportSize.y > 0)
        {
            auto& spec = m_Framebuffer->GetSpec();
            if (spec.Width != (uint32_t)m_ViewportSize.x || spec.Height != (uint32_t)m_ViewportSize.y)
            {
                m_Framebuffer->Resize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
                m_Camera.SetAspectRatio(m_ViewportSize.x / m_ViewportSize.y);
            }
        }

        ImGuiIO& io = ImGui::GetIO();
        // Undo / Redo -----  Ctrl+Z/ Ctrl+Y
        if (!io.WantTextInput)
        {
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) Undo();
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) Redo();
        }

        // Orbite (clic droit, seulement si pas en train de dragger le gizmo)
        if (m_ViewportHovered && ImGui::IsMouseDragging(ImGuiMouseButton_Right))
            m_Camera.Orbit(io.MouseDelta.x * 0.4f, -io.MouseDelta.y * 0.4f);

        // --- Gizmo input (ImGui direct, plus fiable que le systeme d'events) ---
        if (m_ViewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && m_Selected >= 0 && !m_GizmoDragging)
        {
            ImGuiIO& io = ImGui::GetIO();
            glm::vec2 mp = { io.MousePos.x - m_ViewportPos.x, io.MousePos.y - m_ViewportPos.y };
            glm::vec2 ndc = { mp.x / m_ViewportSize.x * 2.0f - 1.0f,
                              1.0f - mp.y / m_ViewportSize.y * 2.0f };
            m_ActiveAxis = HitTestGizmo(ndc);
            m_GizmoDragging = (m_ActiveAxis != GizmoAxis::None);
            if (m_GizmoDragging) SaveSnapshot();
        }

        if (m_GizmoDragging && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && m_Selected >= 0)
        {
            ImGuiIO& io = ImGui::GetIO();
            glm::vec2 delta = { io.MouseDelta.x / m_ViewportSize.x * 2.0f,
                               -io.MouseDelta.y / m_ViewportSize.y * 2.0f };

            glm::vec3& pos = m_Objects[m_Selected].Position;
            glm::vec3 axisDir = (m_ActiveAxis == GizmoAxis::X) ? glm::vec3(1, 0, 0) :
                (m_ActiveAxis == GizmoAxis::Y) ? glm::vec3(0, 1, 0) :
                glm::vec3(0, 0, 1);

            glm::mat4 vp = m_Camera.GetViewProjection();
            auto proj2D = [&](glm::vec3 p) -> glm::vec2 {
                glm::vec4 c = vp * glm::vec4(p, 1.0f);
                return glm::vec2(c.x / c.w, c.y / c.w);
                };

            glm::vec2 screenAxis = proj2D(pos + axisDir) - proj2D(pos);
            float screenLen = glm::length(screenAxis);
            if (screenLen > 0.0001f)
            {
                float t = glm::dot(delta, screenAxis / screenLen) / screenLen;
                pos += axisDir * t;
            }
        }

        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            m_GizmoDragging = false;
            m_ActiveAxis = GizmoAxis::None;
        }

        // Hover highlight
        if (!m_GizmoDragging && m_ViewportHovered && m_Selected >= 0)
        {
            ImGuiIO& io = ImGui::GetIO();
            glm::vec2 mp = { io.MousePos.x - m_ViewportPos.x, io.MousePos.y - m_ViewportPos.y };
            glm::vec2 ndc = { mp.x / m_ViewportSize.x * 2.0f - 1.0f,
                              1.0f - mp.y / m_ViewportSize.y * 2.0f };
            m_HoveredAxis = HitTestGizmo(ndc);
        }
        else if (!m_ViewportHovered && !m_GizmoDragging)
            m_HoveredAxis = GizmoAxis::None;

        // --- Render dans le framebuffer ---
        m_Framebuffer->Bind();
        Purr::RenderCommand::SetClearColor(0.12f, 0.12f, 0.12f, 1.0f);
        Purr::RenderCommand::Clear();

        glm::mat4 invView = glm::inverse(m_Camera.GetViewMatrix());
        glm::vec3 camPos = glm::vec3(invView[3][0], invView[3][1], invView[3][2]);

        auto uploadLights = [&](std::shared_ptr<Purr::Shader>& sh) {
            sh->SetMat4("u_VP", m_Camera.GetViewProjection());
            sh->SetFloat3("u_CamPos", camPos);
            sh->SetFloat("u_AmbientStrength", m_AmbientStrength);
            for (int i = 0; i < 4; i++) {
                std::string b = "u_Lights[" + std::to_string(i) + "].";
                sh->SetFloat3(b + "Position", m_Lights[i].Position);
                sh->SetFloat3(b + "Color", m_Lights[i].Color);
                sh->SetFloat(b + "Intensity", m_Lights[i].Enabled ? m_Lights[i].Intensity : 0.0f);
                sh->SetFloat(b + "Constant", m_Lights[i].Constant);
                sh->SetFloat(b + "Linear", m_Lights[i].Linear);
                sh->SetFloat(b + "Quadratic", m_Lights[i].Quadratic);
            }
            };

        for (auto& obj : m_Objects)
        {
            glm::mat4 model = obj.GetTransform();
            glm::mat4 normalMat = glm::transpose(glm::inverse(model));
            auto& va = obj.IsPlane ? m_PlaneVA : m_VA;

            if (obj.Tex) {
                m_TexShader->Bind();
                uploadLights(m_TexShader);
                m_TexShader->SetMat4("u_Model", model);
                m_TexShader->SetMat4("u_NormalMat", normalMat);
                m_TexShader->SetFloat3("u_MatDiffuse", obj.Mat.Diffuse);
                m_TexShader->SetFloat3("u_MatSpecular", obj.Mat.Specular);
                m_TexShader->SetFloat("u_MatShininess", obj.Mat.Shininess);
                m_TexShader->SetInt("u_IllumModel", (int)obj.Mat.Model);
                m_TexShader->SetInt("u_Texture", 0);
                obj.Tex->Bind(0);
                Purr::RenderCommand::DrawIndexed(va);
            }
            else {
                m_Shader->Bind();
                uploadLights(m_Shader);
                m_Shader->SetMat4("u_Model", model);
                m_Shader->SetMat4("u_NormalMat", normalMat);
                m_Shader->SetFloat3("u_MatAmbient", obj.Mat.Ambient);
                m_Shader->SetFloat3("u_MatDiffuse", obj.Mat.Diffuse);
                m_Shader->SetFloat3("u_MatSpecular", obj.Mat.Specular);
                m_Shader->SetFloat("u_MatShininess", obj.Mat.Shininess);
                m_Shader->SetInt("u_IllumModel", (int)obj.Mat.Model);
                Purr::RenderCommand::DrawIndexed(va);
            }
        }

        // Bounding box
        if (m_Selected >= 0 && m_Selected < (int)m_Objects.size())
        {
            auto& obj = m_Objects[m_Selected];
            glm::mat4 model = glm::translate(glm::mat4(1.0f), obj.Position);
            model = glm::rotate(model, glm::radians(obj.Rotation.x), { 1,0,0 });
            model = glm::rotate(model, glm::radians(obj.Rotation.y), { 0,1,0 });
            model = glm::rotate(model, glm::radians(obj.Rotation.z), { 0,0,1 });
            model = glm::scale(model, obj.Scale * 1.02f);
            m_WireShader->Bind();
            m_WireShader->SetMat4("u_VP", m_Camera.GetViewProjection());
            m_WireShader->SetMat4("u_Model", model);
            m_BBoxVA->Bind();
            Purr::RenderCommand::DrawLines(m_BBoxVA);
        }

        // ---- Gizmo (toujours par-dessus la geometrie) ----
        if (m_Selected >= 0 && m_Selected < (int)m_Objects.size())
        {
            auto& obj = m_Objects[m_Selected];
            float scale = m_Camera.GetRadius() * 0.12f;

            // Rotations pour orienter la fleche +X vers Y et Z
            glm::mat4 rotToY = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0, 0, 1));
            glm::mat4 rotToZ = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0, 1, 0));

            struct AxisDesc { glm::mat4 rot; glm::vec4 color; GizmoAxis axis; };
            AxisDesc axes[3] = {
                { glm::mat4(1.0f), { 1.0f, 0.15f, 0.15f, 1.0f }, GizmoAxis::X },
                { rotToY,          { 0.15f, 1.0f, 0.15f, 1.0f }, GizmoAxis::Y },
                { rotToZ,          { 0.15f, 0.15f, 1.0f, 1.0f }, GizmoAxis::Z },
            };

            Purr::RenderCommand::SetDepthTest(false);
            m_GizmoShader->Bind();
            m_GizmoShader->SetMat4("u_VP", m_Camera.GetViewProjection());

            for (auto& a : axes)
            {
                glm::mat4 model =
                    glm::translate(glm::mat4(1.0f), obj.Position) *
                    a.rot *
                    glm::scale(glm::mat4(1.0f), glm::vec3(scale));

                m_GizmoShader->SetMat4("u_Model", model);

                bool highlight = (m_HoveredAxis == a.axis) || (m_GizmoDragging && m_ActiveAxis == a.axis);
                glm::vec4 col = highlight ? glm::vec4(1.0f, 1.0f, 1.0f, 1.0f) : a.color;
                m_GizmoShader->SetFloat4("u_Color", col);

                Purr::RenderCommand::DrawLines(m_ArrowVA);
            }

            Purr::RenderCommand::SetDepthTest(true);
        }

        m_Framebuffer->Unbind();
    }

    void OnImGuiRender() override
    {
        SetupDockspace();

        // ---- Viewport -----------------------------------------------------
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Viewport");
        m_ViewportHovered = ImGui::IsWindowHovered();

        // Capturer la position du contenu du viewport (pour le hit-test gizmo)
        ImVec2 contentPos = ImGui::GetCursorScreenPos();
        m_ViewportPos = { contentPos.x, contentPos.y };

        ImVec2 size = ImGui::GetContentRegionAvail();
        m_ViewportSize = { size.x, size.y };
        uint64_t texID = m_Framebuffer->GetColorAttachmentID();
        ImGui::Image((ImTextureID)texID, size, ImVec2(0, 1), ImVec2(1, 0));
        ImGui::End();
        ImGui::PopStyleVar();

        // ---- Scene --------------------------------------------------------
        ImGui::Begin("Scene");
        bool isPersp = (m_Camera.GetProjectionMode() == Purr::ProjectionMode::Perspective);
        if (ImGui::RadioButton("Perspective", isPersp))  m_Camera.SetProjectionMode(Purr::ProjectionMode::Perspective);
        ImGui::SameLine();
        if (ImGui::RadioButton("Orthographic", !isPersp)) m_Camera.SetProjectionMode(Purr::ProjectionMode::Orthographic);
        ImGui::Separator();
        ImGui::Text("Objets (%zu)", m_Objects.size());
        for (int i = 0; i < (int)m_Objects.size(); i++) {
            bool sel = (m_Selected == i);
            if (ImGui::Selectable(m_Objects[i].Name.c_str(), sel)) m_Selected = i;
        }
        ImGui::Separator();
        if (ImGui::Button("+ Cube")) {
            SceneObject obj;
            obj.Name = "Cube " + std::to_string(m_Objects.size() + 1);
            obj.Mat.Diffuse = { (float)rand() / RAND_MAX,(float)rand() / RAND_MAX,(float)rand() / RAND_MAX };
            SaveSnapshot();
            m_Objects.push_back(obj);
            m_Selected = (int)m_Objects.size() - 1;
        }
        if (m_Selected >= 0 && m_Selected < (int)m_Objects.size()) {
            ImGui::SameLine();
            if (ImGui::Button("- Supprimer")) {
                SaveSnapshot();
                m_Objects.erase(m_Objects.begin() + m_Selected);
                m_Selected = glm::max(0, m_Selected - 1);
            }
        }



        // ----- Undo / Redo -------------------------------------------------
        ImGui::Separator();
        bool canUndo = !m_UndoStack.empty();
        bool canRedo = !m_RedoStack.empty();


        if (!canUndo) ImGui::BeginDisabled();
        if (ImGui::Button("< Undo")) Undo();
        if (!canUndo) ImGui::EndDisabled();

        ImGui::SameLine();

        if (!canRedo) ImGui::BeginDisabled();
        if (ImGui::Button("> Redo")) Redo();
        if (!canRedo) ImGui::EndDisabled();

        ImGui::End();

    


        // ---- Lumieres -----------------------------------------------------
        ImGui::Begin("Lumieres");
        ImGui::SliderFloat("Ambiance globale", &m_AmbientStrength, 0.0f, 1.0f);
        ImGui::Separator();
        static const char* lightNames[] = { "Lumiere 1","Lumiere 2","Lumiere 3","Lumiere 4" };
        for (int i = 0; i < 4; i++) {
            ImGui::PushID(i);
            ImGui::Checkbox("##en", &m_Lights[i].Enabled); ImGui::SameLine();
            bool open = ImGui::CollapsingHeader(lightNames[i]);
            if (open) {
                ImGui::DragFloat3("Position", glm::value_ptr(m_Lights[i].Position), 0.1f);
                ImGui::ColorEdit3("Couleur", glm::value_ptr(m_Lights[i].Color));
                ImGui::SliderFloat("Intensite", &m_Lights[i].Intensity, 0.0f, 3.0f);
                ImGui::SliderFloat("Lineaire", &m_Lights[i].Linear, 0.0f, 1.0f);
                ImGui::SliderFloat("Quadrat.", &m_Lights[i].Quadratic, 0.0f, 0.5f);
            }
            ImGui::PopID();
        }
        ImGui::End();

        // ---- Properties ---------------------------------------------------
        if (m_Selected >= 0 && m_Selected < (int)m_Objects.size()) {
            SceneObject& obj = m_Objects[m_Selected];
            ImGui::Begin("Properties");
            ImGui::Text("%s", obj.Name.c_str());
            ImGui::Separator();
            ImGui::DragFloat3("Position", glm::value_ptr(obj.Position), 0.05f);
            if (ImGui::IsItemDeactivatedAfterEdit()) SaveSnapshot();
            ImGui::DragFloat3("Rotation", glm::value_ptr(obj.Rotation), 0.5f);
            if (ImGui::IsItemDeactivatedAfterEdit()) SaveSnapshot();
            ImGui::DragFloat3("Scale", glm::value_ptr(obj.Scale), 0.01f, 0.01f, 10.0f);
            if (ImGui::IsItemDeactivatedAfterEdit()) SaveSnapshot();
            ImGui::Separator();
            ImGui::Text("Texture");
            if (obj.Tex && !obj.TexPath.empty()) {
                std::string fn = obj.TexPath.substr(obj.TexPath.find_last_of("/\\") + 1);
                ImGui::TextColored({ 0.5f,1.0f,0.5f,1.0f }, "%s", fn.c_str());
            }
            else { ImGui::TextDisabled("Aucune texture"); }
            if (ImGui::Button("Charger texture...")) {
                std::string path = OpenFileDialog();
                if (!path.empty()) { obj.Tex = std::make_shared<Purr::Texture>(path); obj.TexPath = path; }
            }
            if (obj.Tex) { ImGui::SameLine(); if (ImGui::Button("Retirer")) { obj.Tex = nullptr; obj.TexPath = ""; } }
            ImGui::Separator();
            ImGui::Text("Materiau");
            static const char* presetNames[] = { "Personnalise","Or","Plastique rouge","Caoutchouc" };
            static int presetIdx = 0;
            if (ImGui::Combo("##preset", &presetIdx, presetNames, 4)) {
                switch (presetIdx) {
                case 1: obj.Mat.Ambient = { 0.25f,0.20f,0.07f }; obj.Mat.Diffuse = { 0.75f,0.61f,0.23f }; obj.Mat.Specular = { 0.63f,0.56f,0.37f }; obj.Mat.Shininess = 51.2f; break;
                case 2: obj.Mat.Ambient = { 0.05f,0,0 }; obj.Mat.Diffuse = { 0.5f,0,0 }; obj.Mat.Specular = { 0.7f,0.6f,0.6f }; obj.Mat.Shininess = 32.0f; break;
                case 3: obj.Mat.Ambient = { 0.02f,0.02f,0.02f }; obj.Mat.Diffuse = { 0.01f,0.01f,0.01f }; obj.Mat.Specular = { 0.4f,0.4f,0.4f }; obj.Mat.Shininess = 10.0f; break;
                default: break;
                }
            }
            ImGui::ColorEdit3("Diffuse", glm::value_ptr(obj.Mat.Diffuse));
            ImGui::ColorEdit3("Speculaire", glm::value_ptr(obj.Mat.Specular));
            ImGui::SliderFloat("Brillance", &obj.Mat.Shininess, 1.0f, 256.0f);
            ImGui::Separator();
            ImGui::Text("Modele d'illumination");
            int model = (int)obj.Mat.Model;
            if (ImGui::Combo("##illum", &model, s_ModelNames, 3)) obj.Mat.Model = (IlluminationModel)model;
            ImGui::End();
        }
    }

    void OnEvent(Purr::Event& e) override
    {
        if (!m_ViewportHovered) return;

        Purr::EventDispatcher dispatcher(e);

        // Scroll -> zoom camera
        dispatcher.Dispatch<Purr::MouseScrolledEvent>([this](Purr::MouseScrolledEvent& e) {
            m_Camera.Zoom(e.GetYOffset() * 0.3f);
            return false;
            });

    }

    // Undo / Redo ----------------------------------------------------------------
    void SaveSnapshot()
    {
        m_UndoStack.push_back(m_Objects);
        if ((int)m_UndoStack.size() > k_MaxHistory)
            m_UndoStack.erase(m_UndoStack.begin());
        m_RedoStack.clear();
    }
    void Undo()
    {
        if (m_UndoStack.empty()) return;
        m_RedoStack.push_back(m_Objects);
        m_Objects = m_UndoStack.back();
        m_UndoStack.pop_back();
        m_Selected = glm::clamp(m_Selected, -1, (int)m_Objects.size() - 1);
    }
    void Redo()
    {
        if (m_RedoStack.empty()) return;
        m_UndoStack.push_back(m_Objects);
        m_Objects = m_RedoStack.back();
        m_RedoStack.pop_back();
        m_Selected = glm::clamp(m_Selected, -1, (int)m_Objects.size() - 1);
    }

private:
    // -----------------------------------------------------------------------
    // Hit-test : retourne l'axe du gizmo le plus proche du curseur (NDC)
    // -----------------------------------------------------------------------
    GizmoAxis HitTestGizmo(glm::vec2 mouseNDC)
    {
        if (m_Selected < 0 || m_Selected >= (int)m_Objects.size())
            return GizmoAxis::None;

        glm::vec3 pos = m_Objects[m_Selected].Position;
        float scale = m_Camera.GetRadius() * 0.12f;
        glm::mat4 vp = m_Camera.GetViewProjection();

        auto proj2D = [&](glm::vec3 p) -> glm::vec2 {
            glm::vec4 c = vp * glm::vec4(p, 1.0f);
            return glm::vec2(c.x / c.w, c.y / c.w);
            };

        glm::vec2 base = proj2D(pos);

        struct AxisInfo { glm::vec3 dir; GizmoAxis axis; };
        AxisInfo axes[3] = {
            { {1,0,0}, GizmoAxis::X },
            { {0,1,0}, GizmoAxis::Y },
            { {0,0,1}, GizmoAxis::Z },
        };

        const float kThreshold = 0.05f; // NDC units (~2-3% of screen width)
        float bestDist = kThreshold;
        GizmoAxis result = GizmoAxis::None;

        for (auto& a : axes)
        {
            glm::vec2 tip = proj2D(pos + a.dir * scale);
            // Distance point -> segment [base, tip]
            glm::vec2 ab = tip - base;
            glm::vec2 ap = mouseNDC - base;
            float denom = glm::max(glm::dot(ab, ab), 0.0001f);
            float t = glm::clamp(glm::dot(ap, ab) / denom, 0.0f, 1.0f);
            float d = glm::length(ap - t * ab);
            if (d < bestDist) { bestDist = d; result = a.axis; }
        }
        return result;
    }

    // -----------------------------------------------------------------------
    // Mesh & shader builders
    // -----------------------------------------------------------------------
    void SetupDockspace()
    {
        static bool firstTime = true;
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("DockSpace##main", nullptr, flags);
        ImGui::PopStyleVar(3);

        ImGuiID dockID = ImGui::GetID("MainDockSpace");
        ImGui::DockSpace(dockID, ImVec2(0, 0), ImGuiDockNodeFlags_None);

        if (firstTime)
        {
            firstTime = false;
            ImGui::DockBuilderRemoveNode(dockID);
            ImGui::DockBuilderAddNode(dockID, ImGuiDockNodeFlags_None);
            ImGui::DockBuilderSetNodeSize(dockID, viewport->Size);

            ImGuiID left, rest;
            ImGui::DockBuilderSplitNode(dockID, ImGuiDir_Left, 0.18f, &left, &rest);

            ImGuiID center, right;
            ImGui::DockBuilderSplitNode(rest, ImGuiDir_Right, 0.22f, &right, &center);

            ImGuiID leftTop, leftBottom;
            ImGui::DockBuilderSplitNode(left, ImGuiDir_Up, 0.5f, &leftTop, &leftBottom);

            ImGui::DockBuilderDockWindow("Scene", leftTop);
            ImGui::DockBuilderDockWindow("Lumieres", leftBottom);
            ImGui::DockBuilderDockWindow("Viewport", center);
            ImGui::DockBuilderDockWindow("Properties", right);

            ImGui::DockBuilderFinish(dockID);
        }

        ImGui::End();
    }

    void BuildGizmoShader()
    {
        m_GizmoShader = std::make_shared<Purr::Shader>(
            R"(#version 410 core
            layout(location=0) in vec3 a_Position;
            uniform mat4 u_VP, u_Model;
            void main() { gl_Position = u_VP * u_Model * vec4(a_Position, 1.0); })",

            R"(#version 410 core
            uniform vec4 u_Color;
            out vec4 color;
            void main() { color = u_Color; })"
        );
    }

    // Fleche orientee le long de +X, de 0 a 1.
    // On l'applique avec une matrice de rotation pour avoir Y et Z.
    void BuildArrowMesh()
    {
        //  0 : (0,    0,    0)   <- base
        //  1 : (0.82, 0,    0)   <- debut du cone
        //  2 : (1,    0,    0)   <- tip
        //  3 : (0.82, 0.1,  0)
        //  4 : (0.82,-0.1,  0)
        //  5 : (0.82, 0,    0.1)
        //  6 : (0.82, 0,   -0.1)
        float v[] = {
            0.0f,  0.0f,  0.0f,
            0.82f, 0.0f,  0.0f,
            1.0f,  0.0f,  0.0f,
            0.82f, 0.1f,  0.0f,
            0.82f,-0.1f,  0.0f,
            0.82f, 0.0f,  0.1f,
            0.82f, 0.0f, -0.1f,
        };
        // Paires GL_LINES : shaft + 4 lignes du cone vers le tip
        uint32_t idx[] = {
            0,1,   // shaft
            1,2,   // axe central cone
            3,2,   // cone haut
            4,2,   // cone bas
            5,2,   // cone avant
            6,2,   // cone arriere
        };

        m_ArrowVA = std::make_shared<Purr::VertexArray>();
        auto vb = std::make_shared<Purr::VertexBuffer>(v, sizeof(v));
        vb->SetLayout({ { Purr::ShaderDataType::Float3, "a_Position" } });
        m_ArrowVA->AddVertexBuffer(vb);
        m_ArrowVA->SetIndexBuffer(std::make_shared<Purr::IndexBuffer>(idx, 12));
    }

    void BuildPlaneMesh() {
        float verts[] = {
            -0.5f,0,-0.5f, 0,1,0, 0,0,
             0.5f,0,-0.5f, 0,1,0, 1,0,
             0.5f,0, 0.5f, 0,1,0, 1,1,
            -0.5f,0, 0.5f, 0,1,0, 0,1,
        };
        uint32_t idx[] = { 0,1,2,2,3,0 };
        m_PlaneVA = std::make_shared<Purr::VertexArray>();
        auto vb = std::make_shared<Purr::VertexBuffer>(verts, sizeof(verts));
        vb->SetLayout({ {Purr::ShaderDataType::Float3,"a_Position"},{Purr::ShaderDataType::Float3,"a_Normal"},{Purr::ShaderDataType::Float2,"a_TexCoord"} });
        m_PlaneVA->AddVertexBuffer(vb);
        m_PlaneVA->SetIndexBuffer(std::make_shared<Purr::IndexBuffer>(idx, 6));
    }

    void BuildCubeMesh() {
        float v[] = {
            -0.5f,-0.5f,-0.5f,0,0,-1,0,0,  0.5f,-0.5f,-0.5f,0,0,-1,1,0,  0.5f,0.5f,-0.5f,0,0,-1,1,1, -0.5f,0.5f,-0.5f,0,0,-1,0,1,
            -0.5f,-0.5f, 0.5f,0,0, 1,0,0,  0.5f,-0.5f, 0.5f,0,0, 1,1,0,  0.5f,0.5f, 0.5f,0,0, 1,1,1, -0.5f,0.5f, 0.5f,0,0, 1,0,1,
            -0.5f,-0.5f,-0.5f,-1,0,0,0,0, -0.5f,0.5f,-0.5f,-1,0,0,1,0, -0.5f,0.5f,0.5f,-1,0,0,1,1, -0.5f,-0.5f,0.5f,-1,0,0,0,1,
             0.5f,-0.5f,-0.5f, 1,0,0,0,0,  0.5f,0.5f,-0.5f, 1,0,0,1,0,  0.5f,0.5f,0.5f, 1,0,0,1,1,  0.5f,-0.5f,0.5f, 1,0,0,0,1,
            -0.5f,0.5f,-0.5f,0,1,0,0,0,   0.5f,0.5f,-0.5f,0,1,0,1,0,   0.5f,0.5f,0.5f,0,1,0,1,1,  -0.5f,0.5f,0.5f,0,1,0,0,1,
            -0.5f,-0.5f,-0.5f,0,-1,0,0,0,  0.5f,-0.5f,-0.5f,0,-1,0,1,0, 0.5f,-0.5f,0.5f,0,-1,0,1,1,-0.5f,-0.5f,0.5f,0,-1,0,0,1,
        };
        uint32_t idx[] = { 0,1,2,2,3,0, 4,5,6,6,7,4, 8,9,10,10,11,8, 12,13,14,14,15,12, 16,17,18,18,19,16, 20,21,22,22,23,20 };
        m_VA = std::make_shared<Purr::VertexArray>();
        auto vb = std::make_shared<Purr::VertexBuffer>(v, sizeof(v));
        vb->SetLayout({ {Purr::ShaderDataType::Float3,"a_Position"},{Purr::ShaderDataType::Float3,"a_Normal"},{Purr::ShaderDataType::Float2,"a_TexCoord"} });
        m_VA->AddVertexBuffer(vb);
        m_VA->SetIndexBuffer(std::make_shared<Purr::IndexBuffer>(idx, 36));
    }

    void BuildShader() {
        std::string vs = R"(
            #version 410 core
            layout(location=0) in vec3 a_Position;
            layout(location=1) in vec3 a_Normal;
            layout(location=2) in vec2 a_TexCoord;
            uniform mat4 u_VP, u_Model, u_NormalMat;
            out vec3 v_FragPos; out vec3 v_Normal;
            void main() {
                vec4 wp = u_Model * vec4(a_Position,1.0);
                v_FragPos = vec3(wp); v_Normal = normalize(mat3(u_NormalMat)*a_Normal);
                gl_Position = u_VP * wp;
            })";
        std::string fs = R"(
            #version 410 core
            in vec3 v_FragPos; in vec3 v_Normal;
            struct LightData{vec3 Position,Color;float Intensity,Constant,Linear,Quadratic;};
            uniform LightData u_Lights[4];
            uniform float u_AmbientStrength;
            uniform vec3 u_CamPos,u_MatAmbient,u_MatDiffuse,u_MatSpecular;
            uniform float u_MatShininess; uniform int u_IllumModel;
            out vec4 color;
            void main(){
                vec3 N=normalize(v_Normal),V=normalize(u_CamPos-v_FragPos);
                vec3 r=u_AmbientStrength*u_MatAmbient;
                for(int i=0;i<4;i++){
                    if(u_Lights[i].Intensity<=0.0)continue;
                    vec3 L=normalize(u_Lights[i].Position-v_FragPos);
                    float d=length(u_Lights[i].Position-v_FragPos);
                    float att=u_Lights[i].Intensity/(u_Lights[i].Constant+u_Lights[i].Linear*d+u_Lights[i].Quadratic*d*d);
                    vec3 lc=u_Lights[i].Color*att;
                    r+=max(dot(N,L),0.0)*lc*u_MatDiffuse;
                    if(u_IllumModel==1){vec3 R=reflect(-L,N);r+=pow(max(dot(V,R),0.0),u_MatShininess)*lc*u_MatSpecular;}
                    else if(u_IllumModel==2){vec3 H=normalize(L+V);r+=pow(max(dot(N,H),0.0),u_MatShininess)*lc*u_MatSpecular;}
                }
                color=vec4(r,1.0);
            })";
        m_Shader = std::make_shared<Purr::Shader>(vs, fs);
    }

    void BuildTexturedShader() {
        std::string vs = R"(
            #version 410 core
            layout(location=0) in vec3 a_Position;
            layout(location=1) in vec3 a_Normal;
            layout(location=2) in vec2 a_TexCoord;
            uniform mat4 u_VP,u_Model,u_NormalMat;
            out vec3 v_FragPos,v_Normal; out vec2 v_TexCoord;
            void main(){
                vec4 wp=u_Model*vec4(a_Position,1.0);
                v_FragPos=vec3(wp); v_Normal=normalize(mat3(u_NormalMat)*a_Normal);
                v_TexCoord=a_TexCoord; gl_Position=u_VP*wp;
            })";
        std::string fs = R"(
            #version 410 core
            in vec3 v_FragPos,v_Normal; in vec2 v_TexCoord;
            struct LightData{vec3 Position,Color;float Intensity,Constant,Linear,Quadratic;};
            uniform LightData u_Lights[4];
            uniform float u_AmbientStrength;
            uniform vec3 u_CamPos,u_MatDiffuse,u_MatSpecular;
            uniform float u_MatShininess; uniform int u_IllumModel;
            uniform sampler2D u_Texture;
            out vec4 color;
            void main(){
                vec3 tc=texture(u_Texture,v_TexCoord*4.0).rgb*u_MatDiffuse;
                vec3 N=normalize(v_Normal),V=normalize(u_CamPos-v_FragPos);
                vec3 r=u_AmbientStrength*tc;
                for(int i=0;i<4;i++){
                    if(u_Lights[i].Intensity<=0.0)continue;
                    vec3 L=normalize(u_Lights[i].Position-v_FragPos);
                    float d=length(u_Lights[i].Position-v_FragPos);
                    float att=u_Lights[i].Intensity/(u_Lights[i].Constant+u_Lights[i].Linear*d+u_Lights[i].Quadratic*d*d);
                    vec3 lc=u_Lights[i].Color*att;
                    r+=max(dot(N,L),0.0)*lc*tc;
                    if(u_IllumModel==1){vec3 R=reflect(-L,N);r+=pow(max(dot(V,R),0.0),u_MatShininess)*lc*u_MatSpecular;}
                    else if(u_IllumModel==2){vec3 H=normalize(L+V);r+=pow(max(dot(N,H),0.0),u_MatShininess)*lc*u_MatSpecular;}
                }
                color=vec4(r,1.0);
            })";
        m_TexShader = std::make_shared<Purr::Shader>(vs, fs);
    }

    void BuildWireShader() {
        m_WireShader = std::make_shared<Purr::Shader>(
            R"(#version 410 core
            layout(location=0) in vec3 a_Position;
            uniform mat4 u_VP,u_Model;
            void main(){gl_Position=u_VP*u_Model*vec4(a_Position,1.0);})",
            R"(#version 410 core
            out vec4 color;
            void main(){color=vec4(1,1,1,1);})"
        );
    }

    void BuildBBoxMesh() {
        float v[] = { -0.5f,-0.5f,-0.5f, 0.5f,-0.5f,-0.5f, 0.5f,0.5f,-0.5f, -0.5f,0.5f,-0.5f,
                     -0.5f,-0.5f, 0.5f, 0.5f,-0.5f, 0.5f, 0.5f,0.5f, 0.5f, -0.5f,0.5f, 0.5f };
        uint32_t idx[] = { 0,1,1,2,2,3,3,0, 4,5,5,6,6,7,7,4, 0,4,1,5,2,6,3,7 };
        m_BBoxVA = std::make_shared<Purr::VertexArray>();
        auto vb = std::make_shared<Purr::VertexBuffer>(v, sizeof(v));
        vb->SetLayout({ {Purr::ShaderDataType::Float3,"a_Position"} });
        m_BBoxVA->AddVertexBuffer(vb);
        m_BBoxVA->SetIndexBuffer(std::make_shared<Purr::IndexBuffer>(idx, 24));
    }

private:
    // Scene
    std::vector<SceneObject> m_Objects;
    int   m_Selected = 0;
    Light m_Lights[4];
    float m_AmbientStrength = 0.15f;

    // Renderer
    std::shared_ptr<Purr::VertexArray>  m_VA, m_PlaneVA, m_BBoxVA;
    std::shared_ptr<Purr::Shader>       m_Shader, m_TexShader, m_WireShader;
    std::shared_ptr<Purr::Texture>      m_CheckerTex;
    std::shared_ptr<Purr::Framebuffer>  m_Framebuffer;
    Purr::Camera                        m_Camera;

    // Gizmo
    std::shared_ptr<Purr::VertexArray>  m_ArrowVA;
    std::shared_ptr<Purr::Shader>       m_GizmoShader;
    GizmoAxis m_ActiveAxis = GizmoAxis::None;
    GizmoAxis m_HoveredAxis = GizmoAxis::None;
    bool      m_GizmoDragging = false;

    // Viewport
    glm::vec2 m_ViewportSize = { 1280, 720 };
    glm::vec2 m_ViewportPos = { 0, 0 };
    bool      m_ViewportHovered = false;

    // Undo / Redo
    std::vector<std::vector<SceneObject>> m_UndoStack;
    std::vector<std::vector<SceneObject>> m_RedoStack;
    static constexpr int k_MaxHistory = 50;
};

// -----------------------------------------------------------------------
class Sandbox : public Purr::Application {
public:
    Sandbox() { PushLayer(new ExampleLayer()); }
    ~Sandbox() {}
};

Purr::Application* Purr::CreateApplication() { return new Sandbox(); }