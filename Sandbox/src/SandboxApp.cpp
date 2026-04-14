#include "purrpch.h"
#include <Purr.h>
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "Purr/Events/MouseEvent.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>
#include <Windows.h>
#include <commdlg.h>
#include "nlohmann/json.hpp"
#include "Purr/ObjLoader.h"

using json = nlohmann::json;

// -----------------------------------------------------------------------
// File dialog
// -----------------------------------------------------------------------
static std::string OpenFileDialog(const char* filter = "All Files\0*.*\0")
{
    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Ouvrir un fichier";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameA(&ofn)) return std::string(filename);
    return "";
}
// -----------------------------------------------------------------------
// Structs
// -----------------------------------------------------------------------
enum class IlluminationModel { Lambert = 0, Phong, BlinnPhong };
static const char* s_ModelNames[] = { "Lambert", "Phong", "Blinn-Phong" };

enum class PrimitiveType { Cube = 0, Plane, Triangle, Circle, RegularPolygon, Ellipse, Custom };
static const char* s_PrimNames[] = { "Cube", "Plan", "Triangle", "Cercle", "Polygone regulier", "Ellipse", "Modele OBJ" };

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
    PrimitiveType Type = PrimitiveType::Cube;
    std::shared_ptr<Purr::Texture> Tex = nullptr;
    std::string TexPath = "";
    std::string MeshPath = "";

    glm::mat4 GetTransform() const {
        glm::mat4 t = glm::translate(glm::mat4(1.0f), Position);
        t = glm::rotate(t, glm::radians(Rotation.x), { 1,0,0 });
        t = glm::rotate(t, glm::radians(Rotation.y), { 0,1,0 });
        t = glm::rotate(t, glm::radians(Rotation.z), { 0,0,1 });
        return glm::scale(t, Scale);
    }
};

// -----------------------------------------------------------------------
// Gizmo enums
// -----------------------------------------------------------------------
enum class GizmoAxis { None, X, Y, Z, All };
enum class GizmoMode { Translate, Rotate, Scale };

// -----------------------------------------------------------------------
// ExampleLayer
// -----------------------------------------------------------------------
class ExampleLayer : public Purr::Layer {
public:
    // Constructeur
    ExampleLayer() : Layer("Example"), m_Camera(60.0f, 1280.0f / 720.0f)
    {
        BuildCubeMesh(); BuildPlaneMesh();
        BuildTriangleMesh(); BuildCircleMesh(); BuildRegPolygonMesh(); BuildEllipseMesh();
        BuildShader(); BuildTexturedShader();
        BuildWireShader(); BuildBBoxMesh();
        BuildGizmoShader(); BuildArrowMesh(); BuildRingMesh(); BuildScaleHandleMesh();

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

        SceneObject plane; plane.Name = "Plan (sol)"; plane.Type = PrimitiveType::Plane;
        plane.Position = { 0,-0.5f,0 }; plane.Scale = { 6,1,6 };
        plane.Mat.Model = IlluminationModel::Lambert;
        plane.Tex = m_CheckerTex;
        m_Objects.push_back(plane);

        // inititialisation de la selection 
        m_Selected = 0;
        m_Selection.insert(0);

        m_Lights[0].Position = { 3.0f, 3.0f,  3.0f }; m_Lights[0].Color = { 1.0f,1.0f,1.0f };
        m_Lights[1].Position = { -3.0f, 2.0f,  2.0f }; m_Lights[1].Color = { 1.0f,0.4f,0.4f };
        m_Lights[2].Position = { 0.0f, 4.0f, -3.0f }; m_Lights[2].Color = { 0.4f,0.6f,1.0f };
        m_Lights[3].Position = { 0.0f,-2.0f,  0.0f }; m_Lights[3].Color = { 0.8f,1.0f,0.4f };
        m_Lights[3].Enabled = false;
    }

    // ---- Helpers sélection ----
    bool IsSelected(int i) const { return m_Selection.count(i) > 0; }

    void SetPrimarySelected(int i)
    {
        m_Selected = i;
        m_Selection.clear();
        if (i >= 0 && i < (int)m_Objects.size()) m_Selection.insert(i);
    }

    void ToggleSelected(int i)
    {
        if (i < 0 || i >= (int)m_Objects.size()) return;
        if (m_Selection.count(i)) {
            m_Selection.erase(i);
            if (m_Selected == i)
                m_Selected = m_Selection.empty() ? -1 : *m_Selection.begin();
        }
        else {
            m_Selection.insert(i);
            m_Selected = i;
        }
    }

    void DeleteSelection()
    {
        if (m_Selection.empty()) return;
        SaveSnapshot();
        std::vector<int> sorted(m_Selection.begin(), m_Selection.end());
        std::sort(sorted.rbegin(), sorted.rend());   // erase de la fin
        for (int idx : sorted) m_Objects.erase(m_Objects.begin() + idx);
        m_Selection.clear();
        m_Selected = m_Objects.empty() ? -1 : 0;
        if (m_Selected >= 0) m_Selection.insert(m_Selected);
    }


    void OnAttach() override { Purr::RenderCommand::EnableDepthTest(); }

    void OnUpdate(float dt) override
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
        // Undo / Redo -----  Ctrl+Z/ Ctrl+Y - Raccourcis Clavier
        if (!io.WantTextInput)
        {
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) Undo();
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) Redo();
            if (ImGui::IsKeyPressed(ImGuiKey_T)) m_GizmoMode = GizmoMode::Translate;
            if (ImGui::IsKeyPressed(ImGuiKey_R)) m_GizmoMode = GizmoMode::Rotate;
            if (ImGui::IsKeyPressed(ImGuiKey_S)) m_GizmoMode = GizmoMode::Scale;

            // ---- Supprimer [Del] ----
            if (ImGui::IsKeyPressed(ImGuiKey_Delete) && !m_Selection.empty())
                DeleteSelection();

            // ---- Copier [Ctrl+C] ----
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C) && !m_Selection.empty())
            {
                m_Clipboard.clear();
                std::vector<int> sorted(m_Selection.begin(), m_Selection.end());
                std::sort(sorted.begin(), sorted.end());
                for (int idx : sorted) m_Clipboard.push_back(m_Objects[idx]);
            }

            // ---- Coller [Ctrl+V] ----
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V) && !m_Clipboard.empty())
            {
                SaveSnapshot();
                m_Selection.clear();
                for (auto obj : m_Clipboard)
                {
                    obj.Position += glm::vec3(0.5f, 0.0f, 0.5f);
                    obj.Name += " (copie)";
                    m_Objects.push_back(obj);
                    m_Selection.insert((int)m_Objects.size() - 1);
                }
                m_Selected = *m_Selection.begin();
            }
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
            if (m_GizmoDragging)
            {
                SaveSnapshot();
                if (m_GizmoMode == GizmoMode::Rotate)
                {
                    // Enregistre l'angle initial souris/centre pour la rotation
                    glm::vec3& pos = m_Objects[m_Selected].Position;
                    glm::vec4 clip = m_Camera.GetViewProjection() * glm::vec4(pos, 1.0f);
                    glm::vec2 center2D = { clip.x / clip.w, clip.y / clip.w };
                    m_RotDragLastAngle = atan2f(ndc.y - center2D.y, ndc.x - center2D.x);
                }
            }
        }

        if (m_GizmoDragging && ImGui::IsMouseDragging(ImGuiMouseButton_Left) && m_Selected >= 0)
        {
            ImGuiIO& io = ImGui::GetIO();

            if (m_GizmoMode == GizmoMode::Translate)
            {
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
                    glm::vec3 move = axisDir * t;
                    for (int idx : m_Selection)
                        m_Objects[idx].Position += move;
                }
            }
            else if (m_GizmoMode == GizmoMode::Rotate)
            {
                glm::vec3& pos = m_Objects[m_Selected].Position;
                glm::vec4 clip = m_Camera.GetViewProjection() * glm::vec4(pos, 1.0f);
                glm::vec2 center2D = { clip.x / clip.w, clip.y / clip.w };

                glm::vec2 mp = { io.MousePos.x - m_ViewportPos.x, io.MousePos.y - m_ViewportPos.y };
                glm::vec2 mouseNDC = { mp.x / m_ViewportSize.x * 2.0f - 1.0f,
                                       1.0f - mp.y / m_ViewportSize.y * 2.0f };
                float currentAngle = atan2f(mouseNDC.y - center2D.y, mouseNDC.x - center2D.x);
                float delta = currentAngle - m_RotDragLastAngle;
                // Gestion wrap-around
                if (delta > glm::pi<float>()) delta -= 2.0f * glm::pi<float>();
                if (delta < -glm::pi<float>()) delta += 2.0f * glm::pi<float>();
                m_RotDragLastAngle = currentAngle;

                float degrees = glm::degrees(delta) * 2.0f;
                if (m_ActiveAxis == GizmoAxis::X) m_Objects[m_Selected].Rotation.x += degrees;
                else if (m_ActiveAxis == GizmoAxis::Y) m_Objects[m_Selected].Rotation.y += degrees;
                else if (m_ActiveAxis == GizmoAxis::Z) m_Objects[m_Selected].Rotation.z += degrees;
            }
            else // Scale
            {
                glm::vec2 delta = { io.MouseDelta.x / m_ViewportSize.x * 2.0f,
                                   -io.MouseDelta.y / m_ViewportSize.y * 2.0f };

                glm::vec3& scl = m_Objects[m_Selected].Scale;
                glm::vec3& pos = m_Objects[m_Selected].Position;

                if (m_ActiveAxis == GizmoAxis::All)
                {
                    // Uniform scale: mouvement horizontal total
                    float t = delta.x * 3.0f;
                    float factor = 1.0f + t;
                    for (int idx : m_Selection)
                        m_Objects[idx].Scale = glm::max(m_Objects[idx].Scale * factor, glm::vec3(0.001f));
                }
                else
                {
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
                        float delta_t = t * 2.0f;
                        for (int idx : m_Selection) {
                            if (m_ActiveAxis == GizmoAxis::X) m_Objects[idx].Scale.x = glm::max(m_Objects[idx].Scale.x + delta_t, 0.001f);
                            else if (m_ActiveAxis == GizmoAxis::Y) m_Objects[idx].Scale.y = glm::max(m_Objects[idx].Scale.y + delta_t, 0.001f);
                            else if (m_ActiveAxis == GizmoAxis::Z) m_Objects[idx].Scale.z = glm::max(m_Objects[idx].Scale.z + delta_t, 0.001f);
                        }
                    }
                }
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

            /*
            auto& va = [&]() -> std::shared_ptr<Purr::VertexArray>&{
                switch (obj.Type) {
                case PrimitiveType::Plane:         return m_PlaneVA;
                case PrimitiveType::Triangle:      return m_TriangleVA;
                case PrimitiveType::Circle:        return m_CircleVA;
                case PrimitiveType::RegularPolygon:return m_RegPolygonVA;
                case PrimitiveType::Ellipse:       return m_EllipseVA;
                default:                           return m_VA; // Cube
                }
                }();
            */
            auto va = GetMeshForObject(obj);
            if (!va) continue;  // mesh pas encore chargé ou fichier invalide


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
                //GetMeshForObject(obj);
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
                //GetMeshForObject(obj);
            }
        }

        // Bounding boxes pour tous les sélectionnés
        m_WireShader->Bind();
        m_WireShader->SetMat4("u_VP", m_Camera.GetViewProjection());
        for (int idx : m_Selection)
        {
            if (idx < 0 || idx >= (int)m_Objects.size()) continue;
            auto& obj = m_Objects[idx];
            glm::mat4 model = glm::translate(glm::mat4(1.0f), obj.Position);
            model = glm::rotate(model, glm::radians(obj.Rotation.x), { 1,0,0 });
            model = glm::rotate(model, glm::radians(obj.Rotation.y), { 0,1,0 });
            model = glm::rotate(model, glm::radians(obj.Rotation.z), { 0,0,1 });
            // Couleur différente pour le pivot vs les autres sélectionnés
            model = glm::scale(model, obj.Scale * 1.02f);
            m_WireShader->SetMat4("u_Model", model);
            m_BBoxVA->Bind();
            Purr::RenderCommand::DrawLines(m_BBoxVA);
        }

        // ---- Gizmo (toujours par-dessus la geometrie) ----
        if (m_Selected >= 0 && m_Selected < (int)m_Objects.size())
        {
            auto& obj = m_Objects[m_Selected];
            float scale = m_Camera.GetRadius() * 0.12f;

            Purr::RenderCommand::SetDepthTest(false);
            m_GizmoShader->Bind();
            m_GizmoShader->SetMat4("u_VP", m_Camera.GetViewProjection());

            if (m_GizmoMode == GizmoMode::Translate)
            {
                // Fleches de translation (comportement original)
                glm::mat4 rotToY = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0, 0, 1));
                glm::mat4 rotToZ = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0, 1, 0));

                struct AxisDesc { glm::mat4 rot; glm::vec4 color; GizmoAxis axis; };
                AxisDesc axes[3] = {
                    { glm::mat4(1.0f), { 1.0f, 0.15f, 0.15f, 1.0f }, GizmoAxis::X },
                    { rotToY,          { 0.15f, 1.0f, 0.15f, 1.0f }, GizmoAxis::Y },
                    { rotToZ,          { 0.15f, 0.15f, 1.0f, 1.0f }, GizmoAxis::Z },
                };

                for (auto& a : axes)
                {
                    glm::mat4 model =
                        glm::translate(glm::mat4(1.0f), obj.Position) *
                        a.rot *
                        glm::scale(glm::mat4(1.0f), glm::vec3(scale));

                    m_GizmoShader->SetMat4("u_Model", model);
                    bool highlight = (m_HoveredAxis == a.axis) || (m_GizmoDragging && m_ActiveAxis == a.axis);
                    glm::vec4 col = highlight ? glm::vec4(1.0f, 1.0f, 0.2f, 1.0f) : a.color;
                    m_GizmoShader->SetFloat4("u_Color", col);
                    Purr::RenderCommand::DrawLines(m_ArrowVA);
                }
            }
            else if (m_GizmoMode == GizmoMode::Rotate)
            {
                // X ring: cercle dans le plan YZ  -> rot 90° autour de Y
                // Y ring: cercle dans le plan XZ  -> rot 90° autour de X
                // Z ring: cercle dans le plan XY  -> identite
                glm::mat4 rotX = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0, 1, 0));
                glm::mat4 rotY = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1, 0, 0));
                glm::mat4 rotZ = glm::mat4(1.0f);

                struct RingDesc { glm::mat4 rot; glm::vec4 color; GizmoAxis axis; };
                RingDesc rings[3] = {
                    { rotX, { 1.0f, 0.15f, 0.15f, 1.0f }, GizmoAxis::X },
                    { rotY, { 0.15f, 1.0f, 0.15f, 1.0f }, GizmoAxis::Y },
                    { rotZ, { 0.15f, 0.15f, 1.0f, 1.0f }, GizmoAxis::Z },
                };

                for (auto& r : rings)
                {
                    glm::mat4 model =
                        glm::translate(glm::mat4(1.0f), obj.Position) *
                        r.rot *
                        glm::scale(glm::mat4(1.0f), glm::vec3(scale));

                    m_GizmoShader->SetMat4("u_Model", model);
                    bool highlight = (m_HoveredAxis == r.axis) || (m_GizmoDragging && m_ActiveAxis == r.axis);
                    glm::vec4 col = highlight ? glm::vec4(1.0f, 1.0f, 0.2f, 1.0f) : r.color;
                    m_GizmoShader->SetFloat4("u_Color", col);
                    Purr::RenderCommand::DrawLines(m_RingVA);
                }
            }
            else // Scale
            {
                // Handles axes X/Y/Z : shaft + petit cube au bout
                glm::mat4 rotToY = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0, 0, 1));
                glm::mat4 rotToZ = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0, 1, 0));

                struct AxisDesc { glm::mat4 rot; glm::vec4 color; GizmoAxis axis; };
                AxisDesc axes[3] = {
                    { glm::mat4(1.0f), { 1.0f, 0.15f, 0.15f, 1.0f }, GizmoAxis::X },
                    { rotToY,          { 0.15f, 1.0f, 0.15f, 1.0f }, GizmoAxis::Y },
                    { rotToZ,          { 0.15f, 0.15f, 1.0f, 1.0f }, GizmoAxis::Z },
                };

                for (auto& a : axes)
                {
                    glm::mat4 model =
                        glm::translate(glm::mat4(1.0f), obj.Position) *
                        a.rot *
                        glm::scale(glm::mat4(1.0f), glm::vec3(scale));

                    m_GizmoShader->SetMat4("u_Model", model);
                    bool highlight = (m_HoveredAxis == a.axis) || (m_GizmoDragging && m_ActiveAxis == a.axis);
                    glm::vec4 col = highlight ? glm::vec4(1.0f, 1.0f, 0.2f, 1.0f) : a.color;
                    m_GizmoShader->SetFloat4("u_Color", col);
                    Purr::RenderCommand::DrawLines(m_ScaleHandleVA);
                }

                // Cube central pour scale uniforme
                {
                    float cs = scale * 0.18f;
                    glm::mat4 model =
                        glm::translate(glm::mat4(1.0f), obj.Position) *
                        glm::scale(glm::mat4(1.0f), glm::vec3(cs));

                    m_GizmoShader->SetMat4("u_Model", model);
                    bool highlight = (m_HoveredAxis == GizmoAxis::All) || (m_GizmoDragging && m_ActiveAxis == GizmoAxis::All);
                    glm::vec4 col = highlight ? glm::vec4(1.0f, 1.0f, 0.2f, 1.0f) : glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
                    m_GizmoShader->SetFloat4("u_Color", col);
                    Purr::RenderCommand::DrawLines(m_BBoxVA);
                }
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


        // ---- Curseur dynamique (gizmo) ----------------------------------------
        if (m_GizmoDragging)
        {
            if (m_GizmoMode == GizmoMode::Rotate)
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            else if (m_GizmoMode == GizmoMode::Scale)
            {
                if (m_ActiveAxis == GizmoAxis::All) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
                else if (m_ActiveAxis == GizmoAxis::X) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                else if (m_ActiveAxis == GizmoAxis::Y) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                else ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            }
            else
            {
                if (m_ActiveAxis == GizmoAxis::X) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                else if (m_ActiveAxis == GizmoAxis::Y) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
                else if (m_ActiveAxis == GizmoAxis::Z) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            }
        }
        else if (m_HoveredAxis != GizmoAxis::None)
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }


        // Capturer la position du contenu du viewport (pour le hit-test gizmo)
        ImVec2 contentPos = ImGui::GetCursorScreenPos();
        m_ViewportPos = { contentPos.x, contentPos.y };

        // ---- Toolbar mode gizmo (overlay haut-gauche du viewport) ----------
        {
            ImGui::SetCursorScreenPos(ImVec2(contentPos.x + 8, contentPos.y + 8));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            ImGui::PushStyleColor(ImGuiCol_Button,
                m_GizmoMode == GizmoMode::Translate ? ImVec4(0.9f, 0.4f, 0.7f, 0.9f) : ImVec4(0.25f, 0.25f, 0.25f, 0.85f));
            if (ImGui::Button("  Translate [T]  ")) m_GizmoMode = GizmoMode::Translate;
            ImGui::PopStyleColor();
            ImGui::SameLine(0, 4);
            ImGui::PushStyleColor(ImGuiCol_Button,
                m_GizmoMode == GizmoMode::Rotate ? ImVec4(0.9f, 0.4f, 0.7f, 0.9f) : ImVec4(0.25f, 0.25f, 0.25f, 0.85f));
            if (ImGui::Button("  Rotation [R]  ")) m_GizmoMode = GizmoMode::Rotate;
            ImGui::PopStyleColor();
            ImGui::SameLine(0, 4);
            ImGui::PushStyleColor(ImGuiCol_Button,
                m_GizmoMode == GizmoMode::Scale ? ImVec4(0.9f, 0.4f, 0.7f, 0.9f) : ImVec4(0.25f, 0.25f, 0.25f, 0.85f));
            if (ImGui::Button("  Scale [S]  ")) m_GizmoMode = GizmoMode::Scale;
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
            // Repositionner le curseur pour l'image
            ImGui::SetCursorScreenPos(contentPos);
        }

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

        // ---- Scene list ----
        for (int i = 0; i < (int)m_Objects.size(); i++) {
            ImGui::PushID(i);
            bool sel = IsSelected(i);
            bool pushedColor = (sel && i != m_Selected);  
            if (pushedColor)
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.6f, 0.3f, 0.5f, 0.6f));

            if (ImGui::Selectable(m_Objects[i].Name.c_str(), sel, ImGuiSelectableFlags_AllowOverlap)) {
                if (ImGui::GetIO().KeyCtrl) ToggleSelected(i);
                else SetPrimarySelected(i);  
            }

            if (pushedColor) ImGui::PopStyleColor(); 
            ImGui::PopID();
        }
        ImGui::TextDisabled("Ctrl+Click = multi-selection");


        ImGui::Separator();
        if (ImGui::Button("+ Add"))
            ImGui::OpenPopup("AddPrimPopup");
        if (ImGui::BeginPopup("AddPrimPopup"))
        {
            struct PrimEntry { const char* label; PrimitiveType type; };
            PrimEntry entries[] = {
                { "Cube",              PrimitiveType::Cube          },
                { "Plan",              PrimitiveType::Plane         },
                { "Triangle",          PrimitiveType::Triangle      },
                { "Cercle",            PrimitiveType::Circle        },
                { "Polygone regulier", PrimitiveType::RegularPolygon},
                { "Ellipse",           PrimitiveType::Ellipse       },
            };
            for (auto& e : entries)
            {
                if (ImGui::MenuItem(e.label))
                {
                    SceneObject obj;
                    int count = 0;
                    for (auto& o : m_Objects) if (o.Type == e.type) count++;
                    obj.Name = std::string(e.label) + " " + std::to_string(count + 1);
                    obj.Type = e.type;
                    obj.Mat.Diffuse = { (float)rand() / RAND_MAX,(float)rand() / RAND_MAX,(float)rand() / RAND_MAX };
                    if (e.type == PrimitiveType::Plane) { obj.Scale = { 3,1,3 }; obj.Position = { 0,-0.5f,0 }; }
                    SaveSnapshot();
                    m_Objects.push_back(obj);
                    m_Selected = (int)m_Objects.size() - 1;
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndPopup();
        }
        if (!m_Selection.empty()) {
            ImGui::SameLine();
            std::string delLabel = m_Selection.size() > 1
                ? ("-  Supprimer (" + std::to_string(m_Selection.size()) + ")")
                : "-  Supprimer";
            if (ImGui::Button(delLabel.c_str()))
                DeleteSelection();
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

        ImGui::Separator();

        if (ImGui::Button("Sauver scene")) {
            std::string p = SaveFileDialog();
            if (!p.empty()) SaveScene(p);
        }
        ImGui::SameLine();
        if (ImGui::Button("Charger scene")) {
            std::string p = OpenFileDialog("Scene JSON\0*.json\0All Files\0*.*\0");
            if (!p.empty()) LoadScene(p);
        }

        //ImGui::SameLine();
        ImGui::Separator();

        // ---- Importer OBJ  ----
        if (ImGui::Button("Importer OBJ")) {
            std::string p = OpenFileDialog("Modeles OBJ\0*.obj\0All Files\0*.*\0");
            if (!p.empty()) {
                SceneObject obj;

                // Extraire le nom du fichier sans extension
                std::string filename = p.substr(p.find_last_of("/\\") + 1);
                std::string stem = filename.substr(0, filename.find_last_of('.'));
                // Compter les objets avec le même nom de base pour éviter les doublons
                int count = 0;
                for (auto& o : m_Objects)
                    if (o.Name.rfind(stem, 0) == 0) count++;
                obj.Name = count == 0 ? stem : stem + " " + std::to_string(count + 1);

                obj.Type = PrimitiveType::Custom;
                obj.MeshPath = p;

                std::string texPath;
                Purr::LoadOBJ(p, texPath);
                if (!texPath.empty()) {
                    obj.TexPath = texPath;
                    obj.Tex = std::make_shared<Purr::Texture>(texPath);
                }

                SaveSnapshot();
                m_Objects.push_back(obj);
                m_Selected = (int)m_Objects.size() - 1;
            }
        }


        // ---- Histogramme --------------------------------------------------
        ImGui::Separator();

        if (ImGui::Button("Calculer histogramme"))
            ComputeHistogram();

        if (!m_HistR.empty())
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 p = ImGui::GetCursorScreenPos();
            float W = ImGui::GetContentRegionAvail().x;
            float H = 120.0f;

            // fond
            dl->AddRectFilled(p, ImVec2(p.x + W, p.y + H), IM_COL32(30, 30, 30, 255));

            // normalise sur le max global (ignore bin 0 = fond noir)
            int maxVal = 1;
            for (int i = 1; i < 256; i++) {
                maxVal = std::max(maxVal, m_HistR[i]);
                maxVal = std::max(maxVal, m_HistG[i]);
                maxVal = std::max(maxVal, m_HistB[i]);
            }

            float binW = W / 256.0f;
            for (int i = 0; i < 256; i++) {
                float x0 = p.x + i * binW;
                float x1 = x0 + std::max(1.0f, binW);

                float rH = (m_HistR[i] / (float)maxVal) * H;
                float gH = (m_HistG[i] / (float)maxVal) * H;
                float bH = (m_HistB[i] / (float)maxVal) * H;

                dl->AddRectFilled({ x0, p.y + H - rH }, { x1, p.y + H }, IM_COL32(255, 60, 60, 120));
                dl->AddRectFilled({ x0, p.y + H - gH }, { x1, p.y + H }, IM_COL32(60, 255, 60, 120));
                dl->AddRectFilled({ x0, p.y + H - bH }, { x1, p.y + H }, IM_COL32(60, 60, 255, 120));
            }

            // avance le curseur pour que ImGui sache que l'espace est utilis�
            ImGui::Dummy(ImVec2(W, H));

            ImGui::TextDisabled("R  G  B  (superposes)");
        }

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
                //ImGui::ColorEdit3("Couleur", glm::value_ptr(m_Lights[i].Color));
                ImGui::ColorEdit3("Couleur", glm::value_ptr(m_Lights[i].Color), ImGuiColorEditFlags_PickerHueWheel);


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
                //std::string path = OpenFileDialog();
                std::string path = OpenFileDialog("Images\0*.png;*.jpg;*.jpeg;*.bmp;*.tga\0All Files\0*.*\0");
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
            // RBG / HSV 
            //ImGui::ColorEdit3("Diffuse", glm::value_ptr(obj.Mat.Diffuse));
            ImGui::ColorEdit3("Diffuse", glm::value_ptr(obj.Mat.Diffuse), ImGuiColorEditFlags_PickerHueWheel);


            // ImGui::ColorEdit3("Speculaire", glm::value_ptr(obj.Mat.Specular));
            ImGui::ColorEdit3("Speculaire", glm::value_ptr(obj.Mat.Specular), ImGuiColorEditFlags_PickerHueWheel);


            ImGui::SliderFloat("Brillance", &obj.Mat.Shininess, 1.0f, 256.0f);
            ImGui::Separator();
            ImGui::Text("Modele d'illumination");
            int model = (int)obj.Mat.Model;
            if (ImGui::Combo("##illum", &model, s_ModelNames, 3)) obj.Mat.Model = (IlluminationModel)model;
           
            
            
            
            // ---- Multi-sélection ----
            if (m_Selection.size() > 1)
            {
                ImGui::Separator();
                ImGui::TextColored({ 0.9f,0.5f,0.9f,1.0f },
                    "%zu objets selectionnes", m_Selection.size());

                // Scale uniforme pour tous
                static glm::vec3 s_BulkScale = { 1.0f, 1.0f, 1.0f };
                ImGui::DragFloat3("Scale (tous)", glm::value_ptr(s_BulkScale), 0.01f, 0.01f, 10.0f);
                if (ImGui::IsItemDeactivatedAfterEdit())
                {
                    SaveSnapshot();
                    for (int idx : m_Selection)
                    {
                        m_Objects[idx].Scale *= s_BulkScale;
                        m_Objects[idx].Scale = glm::max(m_Objects[idx].Scale, glm::vec3(0.001f));
                    }
                    s_BulkScale = { 1.0f, 1.0f, 1.0f };
                }

                // Copier / Coller / Supprimer depuis Properties
                ImGui::Separator();
                if (ImGui::Button("Copier selection")) {
                    m_Clipboard.clear();
                    std::vector<int> sorted(m_Selection.begin(), m_Selection.end());
                    std::sort(sorted.begin(), sorted.end());
                    for (int idx : sorted) m_Clipboard.push_back(m_Objects[idx]);
                }
                ImGui::SameLine();
                if (ImGui::Button("Supprimer selection"))
                    DeleteSelection();
            }
            
            
            
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

    void ComputeHistogram()
    {
        auto& spec = m_Framebuffer->GetSpec();
        int w = (int)spec.Width, h = (int)spec.Height;
        if (w <= 0 || h <= 0) return;

        std::vector<uint8_t> pixels;
        m_Framebuffer->Bind();
        Purr::RenderCommand::ReadPixels(0, 0, w, h, pixels);
        m_Framebuffer->Unbind();

        m_HistR.assign(256, 0);
        m_HistG.assign(256, 0);
        m_HistB.assign(256, 0);

        for (int i = 0; i < w * h; i++) {
            m_HistR[pixels[i * 3 + 0]]++;
            m_HistG[pixels[i * 3 + 1]]++;
            m_HistB[pixels[i * 3 + 2]]++;
        }
    }
    void SaveScene(const std::string& path)
    {
        json j;
        j["objects"] = json::array();
        for (auto& obj : m_Objects)
        {
            json o;
            o["name"] = obj.Name;
            o["type"] = (int)obj.Type;
            o["meshPath"] = obj.MeshPath;
            o["texPath"] = obj.TexPath;
            o["pos"] = { obj.Position.x, obj.Position.y, obj.Position.z };
            o["rot"] = { obj.Rotation.x, obj.Rotation.y, obj.Rotation.z };
            o["scale"] = { obj.Scale.x,    obj.Scale.y,    obj.Scale.z };
            o["diffuse"] = { obj.Mat.Diffuse.x,  obj.Mat.Diffuse.y,  obj.Mat.Diffuse.z };
            o["specular"] = { obj.Mat.Specular.x, obj.Mat.Specular.y, obj.Mat.Specular.z };
            o["shininess"] = obj.Mat.Shininess;
            o["illum"] = (int)obj.Mat.Model;
            j["objects"].push_back(o);
        }
        std::ofstream f(path);
        f << j.dump(2);
    }

    static std::string SaveFileDialog(const char* filter = "Scene JSON\0*.json\0")
    {
        char filename[MAX_PATH] = {};
        OPENFILENAMEA ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFilter = filter;
        ofn.lpstrFile = filename;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = "Sauver la scene";
        ofn.lpstrDefExt = "json";
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
        if (GetSaveFileNameA(&ofn)) return std::string(filename);
        return "";
    }

    void LoadScene(const std::string& path)
    {
        std::ifstream f(path);
        if (!f.is_open()) return;
        json j; f >> j;

        m_Objects.clear();
        m_MeshCache.clear();
        m_Selected = m_Objects.empty() ? -1 : 0;

        for (auto& o : j["objects"])
        {
            SceneObject obj;
            obj.Name = o["name"];
            obj.Type = (PrimitiveType)(int)o["type"];
            obj.MeshPath = o.value("meshPath", "");
            obj.TexPath = o.value("texPath", "");
            obj.Position = { o["pos"][0],   o["pos"][1],   o["pos"][2] };
            obj.Rotation = { o["rot"][0],   o["rot"][1],   o["rot"][2] };
            obj.Scale = { o["scale"][0], o["scale"][1], o["scale"][2] };
            obj.Mat.Diffuse = { o["diffuse"][0],  o["diffuse"][1],  o["diffuse"][2] };
            obj.Mat.Specular = { o["specular"][0], o["specular"][1], o["specular"][2] };
            obj.Mat.Shininess = o["shininess"];
            obj.Mat.Model = (IlluminationModel)(int)o["illum"];
            if (!obj.TexPath.empty())
                obj.Tex = std::make_shared<Purr::Texture>(obj.TexPath);
            m_Objects.push_back(obj);
        }

        // regarder la texture (généré procéduralement)
        for (auto& obj : m_Objects)
        {
            if (obj.Type == PrimitiveType::Plane && obj.TexPath.empty())
                obj.Tex = m_CheckerTex;
        }
    }

private:

    std::shared_ptr<Purr::VertexArray> GetMeshForObject(const SceneObject& obj)
    {
        if (obj.Type == PrimitiveType::Custom) {
            auto it = m_MeshCache.find(obj.MeshPath);
            if (it != m_MeshCache.end()) return it->second;

            std::string texPath;
            auto va = Purr::LoadOBJ(obj.MeshPath, texPath);


            if (va) {
                m_MeshCache[obj.MeshPath] = va;
            }
            return va;
        }
        switch (obj.Type) {
        case PrimitiveType::Plane:         return m_PlaneVA;
        case PrimitiveType::Triangle:      return m_TriangleVA;
        case PrimitiveType::Circle:        return m_CircleVA;
        case PrimitiveType::RegularPolygon:return m_RegPolygonVA;
        case PrimitiveType::Ellipse:       return m_EllipseVA;
        default:                           return m_VA; // Cube
        }
    }



    // -----------------------------------------------------------------------
    // Hit-test : dispatche selon le mode courant
    // -----------------------------------------------------------------------
    GizmoAxis HitTestGizmo(glm::vec2 mouseNDC)
    {
        if (m_GizmoMode == GizmoMode::Translate)
            return HitTestTranslateGizmo(mouseNDC);
        else if (m_GizmoMode == GizmoMode::Rotate)
            return HitTestRotateGizmo(mouseNDC);
        else
            return HitTestScaleGizmo(mouseNDC);
    }

    // -----------------------------------------------------------------------
    // Hit-test fleches de translation
    // -----------------------------------------------------------------------
    GizmoAxis HitTestTranslateGizmo(glm::vec2 mouseNDC)
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

        const float kThreshold = 0.05f;
        float bestDist = kThreshold;
        GizmoAxis result = GizmoAxis::None;

        for (auto& a : axes)
        {
            glm::vec2 tip = proj2D(pos + a.dir * scale);
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
    // Hit-test anneaux de rotation
    // -----------------------------------------------------------------------
    GizmoAxis HitTestRotateGizmo(glm::vec2 mouseNDC)
    {
        if (m_Selected < 0 || m_Selected >= (int)m_Objects.size())
            return GizmoAxis::None;

        glm::vec3 pos = m_Objects[m_Selected].Position;
        float scale = m_Camera.GetRadius() * 0.12f;
        glm::mat4 vp = m_Camera.GetViewProjection();

        auto proj2D = [&](glm::vec3 p) -> glm::vec2 {
            glm::vec4 c = vp * glm::vec4(p, 1.0f);
            return { c.x / c.w, c.y / c.w };
            };

        // Les vecteurs u/v de chaque anneau (coherents avec les matrices de rendu)
        // X ring (rot 90° Y): points = (0, sin(a), -cos(a))  -> u=(0,0,-1) v=(0,1,0)
        // Y ring (rot 90° X): points = (cos(a), 0, sin(a))   -> u=(1,0,0)  v=(0,0,1)
        // Z ring (identite):  points = (cos(a), sin(a), 0)   -> u=(1,0,0)  v=(0,1,0)
        struct RingInfo { glm::vec3 u, v; GizmoAxis axis; };
        RingInfo rings[3] = {
            { {0,0,-1}, {0,1,0}, GizmoAxis::X },
            { {1,0,0},  {0,0,1}, GizmoAxis::Y },
            { {1,0,0},  {0,1,0}, GizmoAxis::Z },
        };

        const int N = 32;
        const float kThreshold = 0.04f;
        float bestDist = kThreshold;
        GizmoAxis result = GizmoAxis::None;

        for (auto& ring : rings)
        {
            for (int i = 0; i < N; i++)
            {
                float a0 = 2.0f * glm::pi<float>() * i / N;
                float a1 = 2.0f * glm::pi<float>() * (i + 1) / N;
                glm::vec3 p0 = pos + scale * (cosf(a0) * ring.u + sinf(a0) * ring.v);
                glm::vec3 p1 = pos + scale * (cosf(a1) * ring.u + sinf(a1) * ring.v);
                glm::vec2 s0 = proj2D(p0), s1 = proj2D(p1);

                glm::vec2 ab = s1 - s0, ap = mouseNDC - s0;
                float denom = glm::max(glm::dot(ab, ab), 0.0001f);
                float t = glm::clamp(glm::dot(ap, ab) / denom, 0.0f, 1.0f);
                float d = glm::length(ap - t * ab);
                if (d < bestDist) { bestDist = d; result = ring.axis; }
            }
        }
        return result;
    }

    // -----------------------------------------------------------------------
    // Hit-test handles de scale
    // -----------------------------------------------------------------------
    GizmoAxis HitTestScaleGizmo(glm::vec2 mouseNDC)
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

        // Test cube central (scale uniforme) en priorite
        {
            glm::vec2 center = proj2D(pos);
            float d = glm::length(mouseNDC - center);
            if (d < 0.04f) return GizmoAxis::All;
        }

        // Test shafts X/Y/Z (meme logique que translate)
        glm::vec2 base = proj2D(pos);
        struct AxisInfo { glm::vec3 dir; GizmoAxis axis; };
        AxisInfo axes[3] = {
            { {1,0,0}, GizmoAxis::X },
            { {0,1,0}, GizmoAxis::Y },
            { {0,0,1}, GizmoAxis::Z },
        };

        const float kThreshold = 0.05f;
        float bestDist = kThreshold;
        GizmoAxis result = GizmoAxis::None;

        for (auto& a : axes)
        {
            glm::vec2 tip = proj2D(pos + a.dir * scale);
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

    // Shaft le long de +X (0→0.82) + petit cube wireframe centré à 0.92 (de 0.82 à 1.02)
    void BuildScaleHandleMesh()
    {
        // v0 : origine
        // v1 : debut cube  (0.82, 0, 0)
        // v2-v9 : 8 coins du cube
        //   front face (Z=-0.1): v2(0.82,-0.1,-0.1) v3(1.02,-0.1,-0.1) v4(1.02,0.1,-0.1) v5(0.82,0.1,-0.1)
        //   back  face (Z=+0.1): v6(0.82,-0.1, 0.1) v7(1.02,-0.1, 0.1) v8(1.02,0.1, 0.1) v9(0.82,0.1, 0.1)
        float v[] = {
            0.00f,  0.00f,  0.00f,  // 0 origine
            0.82f,  0.00f,  0.00f,  // 1 base cube
            0.82f, -0.10f, -0.10f,  // 2
            1.02f, -0.10f, -0.10f,  // 3
            1.02f,  0.10f, -0.10f,  // 4
            0.82f,  0.10f, -0.10f,  // 5
            0.82f, -0.10f,  0.10f,  // 6
            1.02f, -0.10f,  0.10f,  // 7
            1.02f,  0.10f,  0.10f,  // 8
            0.82f,  0.10f,  0.10f,  // 9
        };
        uint32_t idx[] = {
            0,1,            // shaft
            2,3, 3,4, 4,5, 5,2,  // face avant
            6,7, 7,8, 8,9, 9,6,  // face arriere
            2,6, 3,7, 4,8, 5,9,  // aretes laterales
        };
        m_ScaleHandleVA = std::make_shared<Purr::VertexArray>();
        auto vb = std::make_shared<Purr::VertexBuffer>(v, sizeof(v));
        vb->SetLayout({ { Purr::ShaderDataType::Float3, "a_Position" } });
        m_ScaleHandleVA->AddVertexBuffer(vb);
        m_ScaleHandleVA->SetIndexBuffer(std::make_shared<Purr::IndexBuffer>(idx, 26));
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

    void BuildTriangleMesh() {
        // Triangle equilateral plat sur XZ
        float v[] = {
             0.0f, 0.0f, -0.5f,   0,1,0,  0.5f,0.0f,
            -0.5f, 0.0f,  0.433f, 0,1,0,  0.0f,1.0f,
             0.5f, 0.0f,  0.433f, 0,1,0,  1.0f,1.0f,
        };
        uint32_t idx[] = { 0,1,2 };
        m_TriangleVA = std::make_shared<Purr::VertexArray>();
        auto vb = std::make_shared<Purr::VertexBuffer>(v, sizeof(v));
        vb->SetLayout({ {Purr::ShaderDataType::Float3,"a_Position"},{Purr::ShaderDataType::Float3,"a_Normal"},{Purr::ShaderDataType::Float2,"a_TexCoord"} });
        m_TriangleVA->AddVertexBuffer(vb);
        m_TriangleVA->SetIndexBuffer(std::make_shared<Purr::IndexBuffer>(idx, 3));
    }

    void BuildCircleMesh(int segments = 32) {
        std::vector<float> verts;
        std::vector<uint32_t> indices;
        // centre
        verts.insert(verts.end(), { 0.0f,0.0f,0.0f, 0.0f,1.0f,0.0f, 0.5f,0.5f });
        for (int i = 0; i < segments; i++) {
            float a = i * 2.0f * glm::pi<float>() / segments;
            float x = 0.5f * cos(a), z = 0.5f * sin(a);
            verts.insert(verts.end(), { x,0.0f,z, 0.0f,1.0f,0.0f, x + 0.5f,z + 0.5f });
        }
        for (int i = 0; i < segments; i++) {
            indices.push_back(0);
            indices.push_back(i + 1);
            indices.push_back((i + 1) % segments + 1);
        }
        m_CircleVA = std::make_shared<Purr::VertexArray>();
        auto vb = std::make_shared<Purr::VertexBuffer>(verts.data(), (uint32_t)(verts.size() * sizeof(float)));
        vb->SetLayout({ {Purr::ShaderDataType::Float3,"a_Position"},{Purr::ShaderDataType::Float3,"a_Normal"},{Purr::ShaderDataType::Float2,"a_TexCoord"} });
        m_CircleVA->AddVertexBuffer(vb);
        m_CircleVA->SetIndexBuffer(std::make_shared<Purr::IndexBuffer>(indices.data(), (uint32_t)indices.size()));
    }

    void BuildRegPolygonMesh(int sides = 6) {
        std::vector<float> verts;
        std::vector<uint32_t> indices;
        verts.insert(verts.end(), { 0.0f,0.0f,0.0f, 0.0f,1.0f,0.0f, 0.5f,0.5f });
        for (int i = 0; i < sides; i++) {
            float a = i * 2.0f * glm::pi<float>() / sides;
            float x = 0.5f * cos(a), z = 0.5f * sin(a);
            verts.insert(verts.end(), { x,0.0f,z, 0.0f,1.0f,0.0f, x + 0.5f,z + 0.5f });
        }
        for (int i = 0; i < sides; i++) {
            indices.push_back(0);
            indices.push_back(i + 1);
            indices.push_back((i + 1) % sides + 1);
        }
        m_RegPolygonVA = std::make_shared<Purr::VertexArray>();
        auto vb = std::make_shared<Purr::VertexBuffer>(verts.data(), (uint32_t)(verts.size() * sizeof(float)));
        vb->SetLayout({ {Purr::ShaderDataType::Float3,"a_Position"},{Purr::ShaderDataType::Float3,"a_Normal"},{Purr::ShaderDataType::Float2,"a_TexCoord"} });
        m_RegPolygonVA->AddVertexBuffer(vb);
        m_RegPolygonVA->SetIndexBuffer(std::make_shared<Purr::IndexBuffer>(indices.data(), (uint32_t)indices.size()));
    }

    void BuildEllipseMesh(int segments = 32) {
        // Demi-axes: X=0.5, Z=0.25 (ratio 2:1)
        std::vector<float> verts;
        std::vector<uint32_t> indices;
        verts.insert(verts.end(), { 0.0f,0.0f,0.0f, 0.0f,1.0f,0.0f, 0.5f,0.5f });
        for (int i = 0; i < segments; i++) {
            float a = i * 2.0f * glm::pi<float>() / segments;
            float x = 0.5f * cos(a), z = 0.25f * sin(a);
            verts.insert(verts.end(), { x,0.0f,z, 0.0f,1.0f,0.0f, x + 0.5f, z * 2.0f + 0.5f });
        }
        for (int i = 0; i < segments; i++) {
            indices.push_back(0);
            indices.push_back(i + 1);
            indices.push_back((i + 1) % segments + 1);
        }
        m_EllipseVA = std::make_shared<Purr::VertexArray>();
        auto vb = std::make_shared<Purr::VertexBuffer>(verts.data(), (uint32_t)(verts.size() * sizeof(float)));
        vb->SetLayout({ {Purr::ShaderDataType::Float3,"a_Position"},{Purr::ShaderDataType::Float3,"a_Normal"},{Purr::ShaderDataType::Float2,"a_TexCoord"} });
        m_EllipseVA->AddVertexBuffer(vb);
        m_EllipseVA->SetIndexBuffer(std::make_shared<Purr::IndexBuffer>(indices.data(), (uint32_t)indices.size()));
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

    // Anneau unite dans le plan XY (z=0), rayon 1, 64 segments.
    // On l'oriente avec une matrice de rotation pour X/Y/Z.
    void BuildRingMesh()
    {
        const int N = 64;
        float v[N * 3];
        for (int i = 0; i < N; i++) {
            float a = 2.0f * glm::pi<float>() * i / N;
            v[i * 3 + 0] = cosf(a);
            v[i * 3 + 1] = sinf(a);
            v[i * 3 + 2] = 0.0f;
        }
        uint32_t idx[N * 2];
        for (int i = 0; i < N; i++) {
            idx[i * 2 + 0] = i;
            idx[i * 2 + 1] = (i + 1) % N;
        }
        m_RingVA = std::make_shared<Purr::VertexArray>();
        auto vb = std::make_shared<Purr::VertexBuffer>(v, sizeof(v));
        vb->SetLayout({ {Purr::ShaderDataType::Float3, "a_Position"} });
        m_RingVA->AddVertexBuffer(vb);
        m_RingVA->SetIndexBuffer(std::make_shared<Purr::IndexBuffer>(idx, N * 2));
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
    std::vector<SceneObject>     m_Objects;
    int                          m_Selected = 0;          // pivot principal (gizmo)
    std::unordered_set<int>      m_Selection;             // tous les sélectionnés
    std::vector<SceneObject>     m_Clipboard;             // copier/coller


    Light m_Lights[4];
    float m_AmbientStrength = 0.15f;

    // Renderer
    std::shared_ptr<Purr::VertexArray>  m_VA, m_PlaneVA, m_BBoxVA;
    std::shared_ptr<Purr::VertexArray>  m_TriangleVA, m_CircleVA, m_RegPolygonVA, m_EllipseVA;
    std::shared_ptr<Purr::Shader>       m_Shader, m_TexShader, m_WireShader;
    std::shared_ptr<Purr::Texture>      m_CheckerTex;
    std::shared_ptr<Purr::Framebuffer>  m_Framebuffer;
    Purr::Camera                        m_Camera;

    // Gizmo
    std::shared_ptr<Purr::VertexArray>  m_ArrowVA;
    std::shared_ptr<Purr::VertexArray>  m_RingVA;
    std::shared_ptr<Purr::VertexArray>  m_ScaleHandleVA;
    std::shared_ptr<Purr::Shader>       m_GizmoShader;
    GizmoMode m_GizmoMode = GizmoMode::Translate;
    GizmoAxis m_ActiveAxis = GizmoAxis::None;
    GizmoAxis m_HoveredAxis = GizmoAxis::None;
    bool      m_GizmoDragging = false;
    float     m_RotDragLastAngle = 0.0f;

    // Viewport
    glm::vec2 m_ViewportSize = { 1280, 720 };
    glm::vec2 m_ViewportPos = { 0, 0 };
    bool      m_ViewportHovered = false;

    // Undo / Redo
    std::vector<std::vector<SceneObject>> m_UndoStack;
    std::vector<std::vector<SceneObject>> m_RedoStack;
    static constexpr int k_MaxHistory = 50;

    // Histogramme
    std::vector<int> m_HistR, m_HistG, m_HistB;

    // Cache de mesh
    std::unordered_map<std::string, std::shared_ptr<Purr::VertexArray>> m_MeshCache;
};

// -----------------------------------------------------------------------
class Sandbox : public Purr::Application {
public:
    Sandbox() { PushLayer(new ExampleLayer()); }
    ~Sandbox() {}
};

Purr::Application* Purr::CreateApplication() { return new Sandbox(); }