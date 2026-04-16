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
#include "Purr/AssimpLoader.h"
#include "Purr/Renderer/TextureManager.h"
#include <algorithm>
#include <filesystem>
#include <cwctype>
#include <limits>
#define GLM_ENABLE_EXPERIMENTAL //ajouté apres avoir etre engeulé par compilateur . 
#include <glm/gtx/matrix_decompose.hpp> // cas gerer parentng

using json = nlohmann::json;

static std::string PathToUtf8(const std::filesystem::path& p)
{
#if defined(__cpp_char8_t)
    std::u8string u8 = p.u8string();
    return std::string(u8.begin(), u8.end());
#else
    return p.u8string();
#endif
}

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

enum class PrimitiveType { Cube = 0, Plane, Triangle, Circle, RegularPolygon, Ellipse, Sphere, Custom, Folder, PolarMarker };
static const char* s_PrimNames[] = { "Cube", "Plan", "Triangle", "Cercle", "Polygone regulier", "Ellipse", "Sphere", "Modele OBJ", "Dossier", "Marqueur (polaire)" };

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

// -----------------------------------------------------------------------
// Script system
// -----------------------------------------------------------------------
class ScriptComponent {
public:
    virtual ~ScriptComponent() = default;
    virtual void OnStart() {}
    virtual void OnUpdate(float dt) {}
    virtual const char* GetName() { return "Script"; }
    virtual const char* GetSource() { return ""; }
    struct SceneObject* Owner = nullptr; // forward declare, assigné au Play
};

struct KeyframeTRS {
    float Time = 0.0f;
    glm::vec3 Position = { 0,0,0 };
    glm::vec3 Rotation = { 0,0,0 };
    glm::vec3 Scale = { 1,1,1 };
};

enum class PlayerAnimState { Idle = 0, Walk, Jump, Fall };

struct SceneObject {
    struct RenderPart {
        std::shared_ptr<Purr::VertexArray> Mesh = nullptr;
        std::shared_ptr<Purr::Texture> Texture = nullptr;
        std::string TexturePath;
        std::vector<std::string> BoneNames;
        std::string AnimNodeName;
        glm::mat4   AnimNodeBindLocal = glm::mat4(1.0f);
        glm::mat4   AnimNodeBindGlobal = glm::mat4(1.0f);
        glm::vec3 DiffuseTint = glm::vec3(1.0f);
        glm::vec3 BoundsMin = glm::vec3(0.0f);
        glm::vec3 BoundsMax = glm::vec3(0.0f);
    };

    std::string Name;
    int ParentIndex = -1;
    glm::vec3 Position = { 0,0,0 }, Rotation = { 0,0,0 }, Scale = { 1,1,1 };
    Material Mat;
    PrimitiveType Type = PrimitiveType::Cube;
    std::shared_ptr<Purr::Texture> Tex = nullptr;
    std::string TexPath = "", MeshPath = "";
    std::vector<RenderPart> Parts;
    std::unique_ptr<ScriptComponent> Script;
    float TexTiling = 1.0f;
    bool IsColliderOnly = false;
    bool HiddenInHierarchy = false;
    float Opacity = 1.0f;
    bool HasAnimation = false;
    bool AnimationPlaying = false;
    bool AnimationLoop = true;
    float AnimationTime = 0.0f;
    float AnimationSpeed = 1.0f;
    std::vector<KeyframeTRS> AnimationKeys;
    bool UseSpring = false;
    glm::vec3 SpringAnchor = { 0,0,0 };
    glm::vec3 SpringVelocity = { 0,0,0 };
    float SpringK = 20.0f;
    float SpringDamping = 5.0f;
    float SpringMass = 1.0f;
    bool UseProceduralTexture = false;
    float ProceduralTexScale = 8.0f;
    /** Coordonnees cylindriques (plan XZ) : x = r*cos(theta), z = r*sin(theta), y = hauteur. Utilise si Type == PolarMarker. */
    float PolarRadius = 0.0f;
    float PolarThetaDeg = 0.0f;

    SceneObject(const SceneObject& o)
        : Name(o.Name), ParentIndex(o.ParentIndex)  // copier ParentIndex
        , Position(o.Position), Rotation(o.Rotation), Scale(o.Scale)
        , Mat(o.Mat), Type(o.Type), Tex(o.Tex), TexPath(o.TexPath), MeshPath(o.MeshPath), Parts(o.Parts)
        , TexTiling(o.TexTiling), IsColliderOnly(o.IsColliderOnly)
        , HiddenInHierarchy(o.HiddenInHierarchy), Opacity(o.Opacity)
        , HasAnimation(o.HasAnimation), AnimationPlaying(o.AnimationPlaying), AnimationLoop(o.AnimationLoop)
        , AnimationTime(o.AnimationTime), AnimationSpeed(o.AnimationSpeed), AnimationKeys(o.AnimationKeys)
        , UseSpring(o.UseSpring), SpringAnchor(o.SpringAnchor), SpringVelocity(o.SpringVelocity)
        , SpringK(o.SpringK), SpringDamping(o.SpringDamping), SpringMass(o.SpringMass)
        , UseProceduralTexture(o.UseProceduralTexture), ProceduralTexScale(o.ProceduralTexScale)
        , PolarRadius(o.PolarRadius), PolarThetaDeg(o.PolarThetaDeg)
        , Script(nullptr) {
    }

    SceneObject& operator=(const SceneObject& o) {
        Name = o.Name; ParentIndex = o.ParentIndex;
        Position = o.Position; Rotation = o.Rotation; Scale = o.Scale;
        Mat = o.Mat; Type = o.Type; Tex = o.Tex; TexPath = o.TexPath; MeshPath = o.MeshPath; Parts = o.Parts;
        TexTiling = o.TexTiling;
        IsColliderOnly = o.IsColliderOnly;
        HiddenInHierarchy = o.HiddenInHierarchy;
        Opacity = o.Opacity;
        HasAnimation = o.HasAnimation;
        AnimationPlaying = o.AnimationPlaying;
        AnimationLoop = o.AnimationLoop;
        AnimationTime = o.AnimationTime;
        AnimationSpeed = o.AnimationSpeed;
        AnimationKeys = o.AnimationKeys;
        UseSpring = o.UseSpring;
        SpringAnchor = o.SpringAnchor;
        SpringVelocity = o.SpringVelocity;
        SpringK = o.SpringK;
        SpringDamping = o.SpringDamping;
        SpringMass = o.SpringMass;
        UseProceduralTexture = o.UseProceduralTexture;
        ProceduralTexScale = o.ProceduralTexScale;
        PolarRadius = o.PolarRadius;
        PolarThetaDeg = o.PolarThetaDeg;
        Script = nullptr; return *this;
    }
    SceneObject() = default;

    void PolarSyncCartesianFromPolar() {
        if (Type != PrimitiveType::PolarMarker) return;
        const float th = glm::radians(PolarThetaDeg);
        Position.x = PolarRadius * cosf(th);
        Position.z = PolarRadius * sinf(th);
    }

    void PolarSyncPolarFromCartesianXZ() {
        if (Type != PrimitiveType::PolarMarker) return;
        PolarRadius = sqrtf(Position.x * Position.x + Position.z * Position.z);
        PolarThetaDeg = glm::degrees(atan2f(Position.z, Position.x));
    }

    glm::mat4 GetTransform() const {
        glm::vec3 pos = Position;
        if (Type == PrimitiveType::PolarMarker) {
            const float th = glm::radians(PolarThetaDeg);
            pos.x = PolarRadius * cosf(th);
            pos.z = PolarRadius * sinf(th);
        }
        glm::mat4 t = glm::translate(glm::mat4(1.0f), pos);
        t = glm::rotate(t, glm::radians(Rotation.x), { 1,0,0 });
        t = glm::rotate(t, glm::radians(Rotation.y), { 0,1,0 });
        t = glm::rotate(t, glm::radians(Rotation.z), { 0,0,1 });
        return glm::scale(t, Scale);
    }


    glm::mat4 GetWorldTransform(const std::vector<SceneObject>& objs) const {
        glm::mat4 world(1.0f);
        world = GetTransform();

        int cur = ParentIndex;
        int guard = 0;
        const int maxDepth = (int)objs.size() + 1;
        while (cur >= 0 && cur < (int)objs.size() && guard < maxDepth) {
            world = objs[cur].GetTransform() * world;
            cur = objs[cur].ParentIndex;
            ++guard;
        }
        return world;
    }

    glm::vec3 GetWorldPosition(const std::vector<SceneObject>& objs) const {
        return glm::vec3(GetWorldTransform(objs)[3]);
    }
};

static float s_PlayCameraAzimuthDeg = 35.0f;
static bool  s_PlayCameraFirstPerson = false;
static bool  s_PlayEmoteLockMovement = false;



class CatScript : public ScriptComponent {
public:
    float Speed = 2.0f;
    float BobAmp = 0.00f;   // amplitude du "bounce" vertical | désactivé pour le moment.
    float BobSpeed = 8.0f;
    float RotSpeed = 90.0f;   // degrés/sec pour la rotation

    void OnStart() override
    {
        m_Time = 0.0f;
    }

    void OnUpdate(float dt) override
    {
        if (s_PlayEmoteLockMovement)
            return;
        glm::vec3 move = { 0,0,0 };
        float az = glm::radians(s_PlayCameraAzimuthDeg);
        glm::vec3 forward = glm::normalize(glm::vec3(-cosf(az), 0.0f, -sinf(az)));
        glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
        auto isMoveKeyDown = [](ImGuiKey imguiKey, int purrKey) {
            return ImGui::IsKeyDown(imguiKey) || Purr::Input::IsKeyPressed(purrKey);
            };

        if (isMoveKeyDown(ImGuiKey_W, PURR_KEY_W)) move += forward;
        if (isMoveKeyDown(ImGuiKey_S, PURR_KEY_S)) move -= forward;
        if (isMoveKeyDown(ImGuiKey_A, PURR_KEY_A)) move -= right;
        if (isMoveKeyDown(ImGuiKey_D, PURR_KEY_D)) move += right;

        bool moving = glm::length(move) > 0.0f;

        if (moving)
        {
            move = glm::normalize(move) * Speed * dt;
            Owner->Position += move;

            // En 1re personne, la souris pilote déjà la rotation (comme Roblox).
            if (!s_PlayCameraFirstPerson) {
                // Rotation vers la direction de mouvement
                // Le mesh Roblox est orienté à l'envers par rapport au repère monde:
                // on ajoute 180° pour que W = avance visuellement "vers l'avant".
                float targetAngle = glm::degrees(atan2f(move.x, move.z)) + 180.0f;
                float diff = targetAngle - Owner->Rotation.y;
                // wrap
                while (diff > 180.0f) diff -= 360.0f;
                while (diff < -180.0f) diff += 360.0f;
                Owner->Rotation.y += diff * RotSpeed * dt * 0.1f;
            }
        }

        // Le Y est geré par la physique globale (gravité + collisions).
        m_Time += dt;
    }

    const char* GetName() override { return "CatScript"; }
    const char* GetSource() override {
        return
            R"(class CatScript : public ScriptComponent {
public:
    float Speed    = 3.0f;
    float BobAmp   = 0.06f;
    float BobSpeed = 8.0f;

    void OnStart() override {
        m_BaseY = Owner->Position.y;
    }

    void OnUpdate(float dt) override {
        glm::vec3 move = {0,0,0};
        if (IsKeyDown(Key::A)) move.x -= 1.0f;
        if (IsKeyDown(Key::D)) move.x += 1.0f;
        if (IsKeyDown(Key::W)) move.z -= 1.0f;
        if (IsKeyDown(Key::S)) move.z += 1.0f;

        if (glm::length(move) > 0.0f) {
            move = glm::normalize(move) * Speed * dt;
            Owner->Position += move;
        }
        Owner->Position.y = m_BaseY
            + sinf(m_Time * BobSpeed) * BobAmp;
        m_Time += dt;
    }
private:
    float m_BaseY = 0.0f;
    float m_Time  = 0.0f;
};)";
    }

private:
    float m_Time = 0.0f;
};
// -----------------------------------------------------------------------
    // Gizmo enums
    // -----------------------------------------------------------------------
    enum class GizmoAxis { None, X, Y, Z, All };
    enum class GizmoMode { Hand, Translate, Rotate, Scale };
enum class PlayerAvatarChoice { Male = 0, Female };

namespace {
constexpr int kMaleEmoteSlotCount = 3;
struct MaleEmoteSlotDef {
    const char* UiLabel;
    const char* RelPath;
};
constexpr MaleEmoteSlotDef kMaleEmoteSlotDefs[kMaleEmoteSlotCount] = {
    { "Hip Hop", "assets/models/roblox/male_player/source/Hip Hop Dancing.fbx" },
    { "Jog", "assets/models/roblox/male_player/source/Jog In Circle.fbx" },
    { "Action pose", "assets/models/roblox/male_player/source/Male Action Pose.fbx" },
};

void SanitizeMaleEmoteClip(LoadedAnimationClip& clip)
{
    for (auto& ch : clip.Channels) {
        ch.PositionKeys.clear();
        ch.ScaleKeys.clear();
    }
    auto& chans = clip.Channels;
    chans.erase(
        std::remove_if(chans.begin(), chans.end(),
            [](const LoadedNodeAnimation& ch) { return ch.NodeName == "metarig"; }),
        chans.end());
}
} // namespace

// -----------------------------------------------------------------------
// ExampleLayer
// -----------------------------------------------------------------------
class ExampleLayer : public Purr::Layer {
public:
    struct MapPreset {
        const char* Label = "";
        const char* MeshPath = "";
        glm::vec3 MapPosition = { 0.0f, 0.0f, 0.0f };
        glm::vec3 MapScale = { 1.0f, 1.0f, 1.0f };
        glm::vec3 PlayerSpawn = { 0.0f, 0.0f, 0.0f }; // utilisé à l'étape gameplay
        glm::vec3 PlayerScale = { 0.5f, 0.5f, 0.5f };
    };

    // Constructeur
    ExampleLayer() : Layer("Example"), m_Camera(60.0f, 1280.0f / 720.0f)
    {
        BuildCubeMesh(); BuildPlaneMesh();
        BuildTriangleMesh(); BuildCircleMesh(); BuildRegPolygonMesh(); BuildEllipseMesh(); BuildSphereMesh();
        BuildShader(); BuildTexturedShader(); BuildSkinnedTexturedShader();
        BuildWireShader(); BuildBBoxMesh();
        BuildGizmoShader(); BuildArrowMesh(); BuildRingMesh(); BuildScaleHandleMesh();

        m_CheckerTex = Purr::TextureManager::Create(128, 128, 0xFFB97FFF, "__checker_default");
        static const char* const kPlayLogoPaths[] = {
            "assets/ui/Play_Purr.png",
            "assets/ui/Purr_Play.png",
        };
        m_PlayButtonTex = nullptr;
        for (const char* playLogoPath : kPlayLogoPaths) {
            auto t = Purr::TextureManager::Load(playLogoPath);
            if (t && t->IsValid()) {
                m_PlayButtonTex = t;
                break;
            }
        }
        static const char* const kStopLogoPaths[] = {
            "assets/ui/Stop_Purr.png",
            "assets/ui/Purr_Stop.png",
        };
        m_StopButtonTex = nullptr;
        for (const char* stopLogoPath : kStopLogoPaths) {
            auto t = Purr::TextureManager::Load(stopLogoPath);
            if (t && t->IsValid()) {
                m_StopButtonTex = t;
                break;
            }
        }

        // Framebuffer
        Purr::FramebufferSpec spec;
        spec.Width = 1280; spec.Height = 720;
        m_Framebuffer = std::make_shared<Purr::Framebuffer>(spec);

        SceneObject plane; plane.Name = "Plan (sol)"; plane.Type = PrimitiveType::Plane;
        plane.TexTiling = 4.0f;
        plane.Position = { 0,-0.5f,0 }; plane.Scale = { 40.0f,1.0f,40.0f };
        plane.Mat.Model = IlluminationModel::Lambert;
        plane.Tex = m_CheckerTex;
        m_Objects.push_back(plane);

        // Pas de selection initiale -> pas de gizmo au demarrage.
        m_Selected = -1;
        m_Selection.clear();

        m_Lights[0].Position = { 3.0f, 3.0f,  3.0f }; m_Lights[0].Color = { 1.0f,1.0f,1.0f };
        m_Lights[1].Position = { -3.0f, 2.0f,  2.0f }; m_Lights[1].Color = { 1.0f,0.4f,0.4f };
        m_Lights[2].Position = { 0.0f, 4.0f, -3.0f }; m_Lights[2].Color = { 0.4f,0.6f,1.0f };
        m_Lights[3].Position = { 0.0f,-2.0f,  0.0f }; m_Lights[3].Color = { 0.8f,1.0f,0.4f };
        m_Lights[3].Enabled = false;
    }

    //  ---- Helper Hierarchie Recursive -----

    void DrawHierarchyNode(int i)
    {
        static const char* s_TypeIcons[] = { "[C]","[P]","[T]","[O]","[N]","[E]","[S]","[M]","[D]","[@]" };
        ImGui::PushID(i);

        if (m_Objects[i].HiddenInHierarchy && !m_ShowHiddenInSceneList) {
            ImGui::PopID();
            return;
        }

        // Trouver les enfants directs
        std::vector<int> children;
        for (int j = 0; j < (int)m_Objects.size(); j++)
            if (m_Objects[j].ParentIndex == i) children.push_back(j);

        if (m_RenamingIndex == i)
        {
            if (m_RenameFocusPending) { ImGui::SetKeyboardFocusHere(0); m_RenameFocusPending = false; }
            ImGui::SetNextItemWidth(-1.0f);
            if (ImGui::InputText("##ren", m_RenameBuffer, sizeof(m_RenameBuffer),
                ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
                CommitRename(i);
            if (ImGui::IsKeyPressed(ImGuiKey_Escape))  m_RenamingIndex = -1;
            if (!ImGui::IsItemActive() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) CommitRename(i);
        }
        else
        {
            bool visInList = !m_Objects[i].HiddenInHierarchy;
            if (ImGui::Checkbox("##listvis", &visInList)) {
                SaveSnapshot();
                m_Objects[i].HiddenInHierarchy = !visInList;
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Afficher / masquer dans cette liste (comme Blender)");
            ImGui::SameLine();

            const char* icon = (int)m_Objects[i].Type < 10 ? s_TypeIcons[(int)m_Objects[i].Type] : "[?]";
            std::string label = std::string(icon) + "  " + m_Objects[i].Name;

            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow;
            if (children.empty()) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

            bool sel = IsSelected(i);
            bool isPivot = (sel && i == m_Selected);
            if (sel) flags |= ImGuiTreeNodeFlags_Selected;

            if (isPivot)     ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.85f, 0.3f, 0.6f, 0.85f));
            else if (sel)    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.55f, 0.25f, 0.45f, 0.6f));

            bool open = ImGui::TreeNodeEx(label.c_str(), flags);

            if (isPivot || sel) ImGui::PopStyleColor();

            // Sélection
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                if (ImGui::GetIO().KeyCtrl) ToggleSelected(i);
                else SetPrimarySelected(i);
            }
            // Renommer
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                m_RenamingIndex = i; m_RenameFocusPending = true;
                strncpy_s(m_RenameBuffer, sizeof(m_RenameBuffer), m_Objects[i].Name.c_str(), _TRUNCATE);
            }

            // ---- Drag source ----
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("HIERARCHY_NODE", &i, sizeof(int));
                ImGui::Text("Deplacer : %s", m_Objects[i].Name.c_str());
                ImGui::EndDragDropSource();
            }
            // ---- Drop target : reparenter ----
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("HIERARCHY_NODE")) {
                    int src = *(const int*)p->Data;
                    if (src != i && !IsAncestor(i, src))   // pas de cycle
                        ReparentKeepWorld(src, i);
                }
                ImGui::EndDragDropTarget();
            }

            // Récursion sur les enfants
            if (open && !children.empty()) {
                for (int child : children) DrawHierarchyNode(child);
                ImGui::TreePop();
            }
        }

        ImGui::PopID();
    }

    // Vérifie si 'candidate' est un ancêtre de 'node' (pour éviter les cycles)
    bool IsAncestor(int candidate, int node) const
    {
        int cur = m_Objects[node].ParentIndex;
        while (cur >= 0) {
            if (cur == candidate) return true;
            cur = m_Objects[cur].ParentIndex;
        }
        return false;
    }

    // helper pour hierarchie parent localisation relative a lui 

    void ReparentKeepWorld(int src, int newParent)
    {
        // Capture la transform monde AVANT de changer le parent
        glm::mat4 worldMat = m_Objects[src].GetWorldTransform(m_Objects);
        m_Objects[src].ParentIndex = newParent;

        // Calcule la nouvelle transform locale
        glm::mat4 newLocal = (newParent >= 0)
            ? glm::inverse(m_Objects[newParent].GetWorldTransform(m_Objects)) * worldMat
            : worldMat;

        // Décompose en position / rotation / scale
        glm::vec3 scale, translation, skew;
        glm::vec4 perspective;
        glm::quat orientation;
        glm::decompose(newLocal, scale, orientation, translation, skew, perspective);

        m_Objects[src].Position = translation;
        m_Objects[src].Scale = scale;
        m_Objects[src].Rotation = glm::degrees(glm::eulerAngles(orientation));
    }

    void GroupSelectionUnderPivot()
    {
        if (m_Selection.size() < 2) return;
        int parent = m_Selected;
        if (parent < 0 || !m_Selection.count(parent))
            parent = *m_Selection.begin();

        std::vector<int> sorted(m_Selection.begin(), m_Selection.end());
        std::sort(sorted.begin(), sorted.end());

        SaveSnapshot();
        for (int ch : sorted) {
            if (ch == parent) continue;
            if (IsAncestor(ch, parent)) continue;
            ReparentKeepWorld(ch, parent);
        }
        SetPrimarySelected(parent);
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

    void SanitizeSelectionState()
    {
        std::unordered_set<int> cleaned;
        for (int idx : m_Selection) {
            if (idx >= 0 && idx < (int)m_Objects.size())
                cleaned.insert(idx);
        }
        m_Selection.swap(cleaned);

        if (m_Selected < 0 || m_Selected >= (int)m_Objects.size())
            m_Selected = -1;

        if (m_Selected == -1) {
            if (!m_Selection.empty())
                m_Selected = *m_Selection.begin();
        }
        else if (!m_Selection.count(m_Selected)) {
            m_Selection.insert(m_Selected);
        }
    }

    void CommitRename(int i)
    {
        if (strlen(m_RenameBuffer) > 0 && i >= 0 && i < (int)m_Objects.size()) {
            SaveSnapshot();
            m_Objects[i].Name = m_RenameBuffer;
        }
        m_RenamingIndex = -1;
    }

    void BuildAnimationDemoOnSelection()
    {
        if (m_Selected < 0 || m_Selected >= (int)m_Objects.size())
            return;

        SaveSnapshot();
        SceneObject& parent = m_Objects[m_Selected];

        // Parent: translation + rotation avec interpolation.
        parent.HasAnimation = true;
        parent.AnimationLoop = true;
        parent.AnimationPlaying = true;
        parent.AnimationSpeed = 1.0f;
        parent.AnimationTime = 0.0f;
        parent.AnimationKeys.clear();
        {
            KeyframeTRS k0; k0.Time = 0.0f; k0.Position = parent.Position; k0.Rotation = parent.Rotation; k0.Scale = parent.Scale;
            KeyframeTRS k1 = k0; k1.Time = 1.2f; k1.Position += glm::vec3(1.2f, 0.25f, 0.0f); k1.Rotation.y += 60.0f;
            KeyframeTRS k2 = k0; k2.Time = 2.4f; k2.Position += glm::vec3(0.0f, 0.0f, 1.2f); k2.Rotation.y += 120.0f;
            KeyframeTRS k3 = k0; k3.Time = 3.6f; k3.Position = k0.Position; k3.Rotation = k0.Rotation;
            parent.AnimationKeys = { k0, k1, k2, k3 };
        }

        // Enfant de démo: animation locale propre pour montrer le graphe de scène.
        SceneObject child;
        child.Name = "AnimChild Demo";
        child.Type = PrimitiveType::Cube;
        child.ParentIndex = m_Selected;
        child.Position = { 0.8f, 0.0f, 0.0f };
        child.Scale = { 0.4f, 0.4f, 0.4f };
        child.Mat.Diffuse = { 0.2f, 0.9f, 1.0f };
        child.HasAnimation = true;
        child.AnimationLoop = true;
        child.AnimationPlaying = true;
        child.AnimationSpeed = 1.0f;
        child.AnimationTime = 0.0f;
        {
            KeyframeTRS c0; c0.Time = 0.0f; c0.Position = child.Position; c0.Rotation = child.Rotation; c0.Scale = child.Scale;
            KeyframeTRS c1 = c0; c1.Time = 0.8f; c1.Position.y += 0.45f; c1.Scale = child.Scale * glm::vec3(1.0f, 1.35f, 1.0f);
            KeyframeTRS c2 = c0; c2.Time = 1.6f; c2.Position = c0.Position;
            child.AnimationKeys = { c0, c1, c2 };
        }
        m_Objects.push_back(child);
    }

    std::shared_ptr<Purr::VertexArray> BuildVertexArrayFromLoadedMesh(const LoadedMesh& lm)
    {
        if (lm.Vertices.empty() || lm.Indices.empty())
            return nullptr;

        std::vector<float> raw;
        raw.reserve(lm.Vertices.size() * 16);
        for (const auto& v : lm.Vertices) {
            raw.push_back(v.Position.x); raw.push_back(v.Position.y); raw.push_back(v.Position.z);
            raw.push_back(v.Normal.x);   raw.push_back(v.Normal.y);   raw.push_back(v.Normal.z);
            raw.push_back(v.TexCoords.x); raw.push_back(v.TexCoords.y);
            raw.push_back((float)v.BoneIDs.x); raw.push_back((float)v.BoneIDs.y);
            raw.push_back((float)v.BoneIDs.z); raw.push_back((float)v.BoneIDs.w);
            raw.push_back(v.BoneWeights.x); raw.push_back(v.BoneWeights.y);
            raw.push_back(v.BoneWeights.z); raw.push_back(v.BoneWeights.w);
        }

        auto va = std::make_shared<Purr::VertexArray>();
        auto vb = std::make_shared<Purr::VertexBuffer>(raw.data(), (uint32_t)(raw.size() * sizeof(float)));
        vb->SetLayout({
            { Purr::ShaderDataType::Float3, "a_Position" },
            { Purr::ShaderDataType::Float3, "a_Normal"   },
            { Purr::ShaderDataType::Float2, "a_TexCoord" },
            { Purr::ShaderDataType::Float4, "a_BoneIDs" },
            { Purr::ShaderDataType::Float4, "a_BoneWeights" }
            });
        va->AddVertexBuffer(vb);
        // IndexBuffer prend un pointeur non-const, on copie depuis le mesh source const.
        std::vector<uint32_t> indices = lm.Indices;
        va->SetIndexBuffer(std::make_shared<Purr::IndexBuffer>(indices.data(), (uint32_t)indices.size()));
        return va;
    }

    std::string ResolveImportedTexturePath(const std::string& modelPath, const std::string& texPath)
    {
        try {
            if (texPath.empty()) return "";
            namespace fs = std::filesystem;
            std::error_code ec;
            fs::path p(texPath);
            if (p.is_absolute()) {
                if (fs::exists(p, ec) && !ec)
                    return PathToUtf8(p);
                // Chemin absolu invalide (souvent exporte depuis une autre machine):
                // on conserve le nom de fichier pour tenter une resolution locale.
                p = p.filename();
            }
            fs::path base(modelPath);
            fs::path candidate = base.parent_path() / p;
            ec.clear();
            if (fs::exists(candidate, ec) && !ec)
                return PathToUtf8(candidate);

            // Fallback: certains packs separent "source" et "textures".
            const fs::path fileOnly = p.filename();
            fs::path modelDir = base.parent_path();
            fs::path cursor = modelDir;
            for (int i = 0; i < 5 && !cursor.empty(); ++i) {
                fs::path texDir = cursor / "textures";
                ec.clear();
                if (fs::exists(texDir, ec) && !ec && fs::is_directory(texDir, ec)) {
                    fs::path texCandidate = texDir / fileOnly;
                    ec.clear();
                    if (fs::exists(texCandidate, ec) && !ec)
                        return PathToUtf8(texCandidate);
                }
                texDir = cursor / "texture";
                ec.clear();
                if (fs::exists(texDir, ec) && !ec && fs::is_directory(texDir, ec)) {
                    fs::path texCandidate = texDir / fileOnly;
                    ec.clear();
                    if (fs::exists(texCandidate, ec) && !ec)
                        return PathToUtf8(texCandidate);
                }
                cursor = cursor.parent_path();
            }
        }
        catch (const std::exception& e) {
            PURR_WARN("ResolveImportedTexturePath exception: {}", e.what());
            return "";
        }
        return "";
    }

    std::string GuessDiffuseTextureNearModel(const std::string& modelPath)
    {
        namespace fs = std::filesystem;
        fs::path model(modelPath);
        fs::path dir = model.parent_path();
        if (!fs::exists(dir) || !fs::is_directory(dir))
            return "";

        auto isTextureExtW = [](const std::wstring& ext) {
            return ext == L".png" || ext == L".jpg" || ext == L".jpeg" || ext == L".tga" || ext == L".bmp";
            };
        auto lowerW = [](std::wstring s) {
            std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return (wchar_t)towlower(c); });
            return s;
            };

        auto scanDirForDiffuse = [&](const fs::path& scanDir, fs::path& best, uintmax_t& bestSize) {
            std::error_code ec;
            if (!fs::exists(scanDir, ec) || ec || !fs::is_directory(scanDir, ec))
                return;
            for (const auto& e : fs::directory_iterator(scanDir, ec)) {
                if (ec) break;
                if (!e.is_regular_file()) continue;
                std::wstring ext = lowerW(e.path().extension().wstring());
                if (!isTextureExtW(ext)) continue;
                std::wstring name = lowerW(e.path().filename().wstring());
                // Priorite aux maps diffuse/basecolor.
                if (name.find(L"_diff") == std::wstring::npos &&
                    name.find(L"diffuse") == std::wstring::npos &&
                    name.find(L"albedo") == std::wstring::npos &&
                    name.find(L"basecolor") == std::wstring::npos)
                    continue;
                uintmax_t sz = 0;
                ec.clear();
                sz = fs::file_size(e.path(), ec);
                if (!ec && sz >= bestSize) {
                    bestSize = sz;
                    best = e.path();
                }
            }
            };

        fs::path best;
        uintmax_t bestSize = 0;
        fs::path cursor = dir;
        for (int depth = 0; depth < 5; ++depth) {
            scanDirForDiffuse(cursor, best, bestSize);
            scanDirForDiffuse(cursor / "textures", best, bestSize);
            scanDirForDiffuse(cursor / "texture", best, bestSize);
            if (!best.empty())
                break;
            fs::path parent = cursor.parent_path();
            if (parent == cursor || parent.empty())
                break;
            cursor = parent;
        }

        try {
            return best.empty() ? "" : PathToUtf8(best);
        }
        catch (const std::exception& e) {
            PURR_WARN("GuessDiffuseTextureNearModel exception: {}", e.what());
            return "";
        }
    }

    std::string GetSelectedPlayerMeshPath() const
    {
        namespace fs = std::filesystem;
        std::error_code ec;
        const std::string& preferred = (m_PlayerAvatarChoice == PlayerAvatarChoice::Female)
            ? m_PlayerFemaleMeshPath
            : m_PlayerMaleMeshPath;
        ec.clear();
        if (!preferred.empty() && fs::exists(preferred, ec) && !ec)
            return preferred;
        ec.clear();
        if (!m_PlayerMaleMeshPath.empty() && fs::exists(m_PlayerMaleMeshPath, ec) && !ec)
            return m_PlayerMaleMeshPath;
        return "assets/models/roblox/baconHair1Tex.obj";
    }

    glm::vec3 GetAvatarScaleMultiplier() const
    {
        return (m_PlayerAvatarChoice == PlayerAvatarChoice::Male) ? m_MaleAvatarScaleMultiplier : m_FemaleAvatarScaleMultiplier;
    }

    glm::vec3 GetAvatarSpawnOffset() const
    {
        return (m_PlayerAvatarChoice == PlayerAvatarChoice::Male) ? m_MaleAvatarSpawnOffset : m_FemaleAvatarSpawnOffset;
    }

    glm::vec3 GetCurrentPlayerBaseScale() const
    {
        // Le male utilise un scale absolu cible (demande gameplay), le female suit le preset map.
        if (m_PlayerAvatarChoice == PlayerAvatarChoice::Male)
            return glm::vec3(0.091f, 0.091f, 0.091f);
        return glm::max(m_CurrentPlayerScale * GetAvatarScaleMultiplier(), glm::vec3(0.01f));
    }

    void TryLoadMaleEmoteClips()
    {
        for (int i = 0; i < kMaleEmoteSlotCount; ++i) {
            m_MaleEmoteClipLoaded[i] = false;
            m_MaleEmoteClips[i] = {};
            LoadedAnimatedAsset animAsset = LoadAnimatedAsset(kMaleEmoteSlotDefs[i].RelPath);
            if (animAsset.Animations.empty()) {
                PURR_WARN("Male emote clip missing or invalid: {}", kMaleEmoteSlotDefs[i].RelPath);
                continue;
            }
            m_MaleEmoteClips[i] = animAsset.Animations.front();
            SanitizeMaleEmoteClip(m_MaleEmoteClips[i]);
            m_MaleEmoteClipLoaded[i] = true;
            PURR_INFO("Male emote [{}] loaded: '{}' channels={}",
                kMaleEmoteSlotDefs[i].UiLabel, m_MaleEmoteClips[i].Name, (int)m_MaleEmoteClips[i].Channels.size());
        }
    }

    void ImportFBXGrouped(const std::string& path)
    {
        auto animated = LoadAnimatedAsset(path);
        auto& meshes = animated.Meshes;
        if (meshes.empty())
            return;

        SaveSnapshot();
        std::string filename = path.substr(path.find_last_of("/\\") + 1);
        std::string stem = filename.substr(0, filename.find_last_of('.'));

        glm::vec3 bmin(std::numeric_limits<float>::max());
        glm::vec3 bmax(std::numeric_limits<float>::lowest());
        bool hasBounds = false;
        for (const auto& lm : meshes) {
            for (const auto& v : lm.Vertices) {
                bmin = glm::min(bmin, v.Position);
                bmax = glm::max(bmax, v.Position);
                hasBounds = true;
            }
        }
        glm::vec3 center(0.0f);
        float importScale = 1.0f;
        if (hasBounds) {
            center = (bmin + bmax) * 0.5f;
            glm::vec3 ext = bmax - bmin;
            float maxDim = glm::max(ext.x, glm::max(ext.y, ext.z));
            constexpr float kTargetMaxDim = 4.0f;
            if (maxDim > kTargetMaxDim && maxDim > 1e-4f)
                importScale = kTargetMaxDim / maxDim; // downscale only
            PURR_INFO("FBX import: bounds min=({}, {}, {}), max=({}, {}, {}), autoScale={}",
                bmin.x, bmin.y, bmin.z, bmax.x, bmax.y, bmax.z, importScale);
        }

        SceneObject folder;
        folder.Name = stem;
        folder.Type = PrimitiveType::Folder;
        folder.Scale = glm::vec3(importScale);
        folder.Position = hasBounds ? (-center * importScale) : glm::vec3(0.0f);
        m_Objects.push_back(folder);
        int folderIdx = (int)m_Objects.size() - 1;
        const std::string fallbackDiffuse = GuessDiffuseTextureNearModel(path);
        if (!fallbackDiffuse.empty())
            PURR_INFO("FBX import: diffuse candidate detectee (fallback desactive par defaut): {}", fallbackDiffuse);

        for (size_t i = 0; i < meshes.size(); ++i) {
            SceneObject obj;
            obj.Type = PrimitiveType::Custom;
            obj.ParentIndex = folderIdx;
            obj.MeshPath = path;
            obj.Position = { 0,0,0 };
            obj.Scale = { 1,1,1 };
            obj.Mat.Diffuse = { 1.0f,1.0f,1.0f };
            obj.Mat.Model = IlluminationModel::Phong;
            obj.Opacity = 1.0f;
            obj.Name = stem + " Part " + std::to_string((int)i + 1);

            SceneObject::RenderPart part;
            part.Mesh = BuildVertexArrayFromLoadedMesh(meshes[i]);
            for (const auto& b : meshes[i].Bones)
                part.BoneNames.push_back(b.Name);
            part.AnimNodeName = meshes[i].SourceNodeName;
            part.AnimNodeBindLocal = meshes[i].SourceNodeBindLocal;
            part.AnimNodeBindGlobal = meshes[i].SourceNodeBindGlobal;
            part.DiffuseTint = glm::vec3(1.0f);
            std::string resolvedTex = ResolveImportedTexturePath(path, meshes[i].TexturePath);
            // Important: ne pas appliquer un fallback global a toutes les parts.
            // Sinon, on obtient des couleurs incoherentes (textures "aleatoires").
            // On garde le fallback seulement si le FBX n'a qu'un seul mesh.
            if (resolvedTex.empty() && meshes.size() == 1)
                resolvedTex = fallbackDiffuse;
            part.TexturePath = resolvedTex;
            if (!resolvedTex.empty())
                part.Texture = Purr::TextureManager::Load(resolvedTex);
            if (part.Texture)
                PURR_INFO("FBX import: part {} texture OK: {}", (int)i + 1, resolvedTex);
            else
                PURR_WARN("FBX import: part {} sans texture (material path='{}').", (int)i + 1, meshes[i].TexturePath);
            obj.Parts.push_back(part);
            if (part.Texture) {
                obj.Tex = part.Texture;
                obj.TexPath = part.TexturePath;
            }
            m_Objects.push_back(obj);
        }

        SetPrimarySelected(folderIdx);
    }

    bool IsPlayFirstPerson() const { return m_PlayCamDistance <= m_PlayCamFPThreshold; }

    void EnterPlayMode()
    {
        m_SavedScene = m_Objects;
        m_SavedCamera.Mode = m_Camera.GetProjectionMode();
        m_SavedCamera.Target = m_Camera.GetTarget();
        m_SavedCamera.Radius = m_Camera.GetRadius();
        m_SavedCamera.Azimuth = m_Camera.GetAzimuth();
        m_SavedCamera.Elevation = m_Camera.GetElevation();
        m_State = EngineState::Playing;
        m_PlayEmoteMenuOpen = false;
        m_PlayMaleEmoteSlotIndex = -1;
        m_PlayMaleEmoteTime = 0.0f;
        m_PlayEmoteDebugTimer = 0.0f;
        s_PlayEmoteLockMovement = false;
        if (m_PlayerAvatarChoice == PlayerAvatarChoice::Male)
            TryLoadMaleEmoteClips();
        m_GizmoDragging = false;
        m_ActiveAxis = GizmoAxis::None;

        // Priorité spawn: marker explicite dans la scène.
        glm::vec3 markerSpawnPos = {};
        bool hasSpawnMarker = TryGetSpawnMarkerSpawnPoint(markerSpawnPos);

        // --- Spawn du personnage Roblox ---
        SceneObject player;
        player.Name = "Player";
        player.Type = PrimitiveType::Custom;
        player.MeshPath = GetSelectedPlayerMeshPath();
        PURR_INFO("Play avatar: {}", player.MeshPath);
        player.Position = (hasSpawnMarker ? markerSpawnPos : m_CurrentMapSpawn) + GetAvatarSpawnOffset();
        player.Scale = GetCurrentPlayerBaseScale();
        player.Mat.Diffuse = { 1.0f, 1.0f, 1.0f };  // blanc = texture pure
        player.Mat.Specular = { 0.3f, 0.3f, 0.3f };
        player.Mat.Shininess = 32.0f;
        player.Mat.Model = IlluminationModel::Phong;
        m_Objects.push_back(player);

        int playerIdx = (int)m_Objects.size() - 1;

        // Forcer le chargement mesh + texture maintenant (pas au premier frame)
        GetMeshForObject(m_Objects[playerIdx]);
        BuildPlayStaticColliders(playerIdx);
        if (m_EnableAutoSpawnSnap)
            m_Objects[playerIdx].Position = ComputeGroundedSpawn(m_Objects[playerIdx].Position, m_Objects[playerIdx].Scale);
        m_PlayerVelocity = { 0.0f, 0.0f, 0.0f };
        m_PlayerOnGround = false;
        for (int i = 0; i < 120 && !m_PlayerOnGround; i++)
            ResolvePlayerPhysics(m_Objects[playerIdx], 0.016f);
        m_PlayerVelocity = { 0.0f, 0.0f, 0.0f };
        PURR_CORE_INFO("Play spawn source={} colliders={} -> x:{:.3f} y:{:.3f} z:{:.3f}",
            hasSpawnMarker ? "marker" : (m_EnableAutoSpawnSnap ? "autosnap" : "preset"),
            (int)m_PlayStaticColliders.size(),
            m_Objects[playerIdx].Position.x, m_Objects[playerIdx].Position.y, m_Objects[playerIdx].Position.z);
        m_Camera.SetProjectionMode(Purr::ProjectionMode::Perspective);
        // Vue Play type Roblox : demarrage en 1re personne, molette pour eloigner (3e personne).
        m_PlayCamDistance = glm::clamp(0.22f, m_PlayCamMinDist, m_PlayCamMaxDist);
        m_PlayCamTargetHeight = glm::max(0.26f, 0.82f * m_CurrentPlayerScale.y);
        m_PlayFPYawDeg = m_Objects[playerIdx].Rotation.y + 180.0f;
        m_PlayFPPitchDeg = 0.0f;
        m_PlayCamAzimuth = m_PlayFPYawDeg;
        m_PlayCamElevation = 18.0f;
        s_PlayCameraAzimuthDeg = m_PlayFPYawDeg;
        m_PlayCameraPrevFirstPerson = false;
        m_PlayerBridgeTime = 0.0f;
        m_SkinnedAnimTime = 0.0f;
        s_PlayCameraFirstPerson = IsPlayFirstPerson();
        UpdatePlayCamera(m_Objects[playerIdx], 0.016f);

        m_PlayBazookaEquipped = false;
        // Important: le push du bazooka peut reallouer m_Objects.
        // On attache donc le script APRES pour ne pas le perdre (copy ctor => Script=nullptr).
        m_Objects[playerIdx].Script = std::make_unique<CatScript>();
        m_Objects[playerIdx].Script->Owner = &m_Objects[playerIdx];
        m_Objects[playerIdx].Script->OnStart();
        m_Projectiles.clear();
        m_BazookaCooldownTimer = 0.0f;

        // Scripts éventuels sur les autres objets
        for (int i = 0; i < playerIdx; i++)
            if (m_Objects[i].Script) {
                m_Objects[i].Script->Owner = &m_Objects[i];
                m_Objects[i].Script->OnStart();
            }
    }

    void StopPlayMode()
    {
        s_PlayCameraFirstPerson = false;
        m_PlayCameraPrevFirstPerson = false;
        RestorePlayCursorAfterPlay();
        m_Objects = m_SavedScene;
        m_State = EngineState::Editor;
        m_Camera.SetProjectionMode(m_SavedCamera.Mode);
        m_Camera.SetOrbitAngles(m_SavedCamera.Azimuth, m_SavedCamera.Elevation);
        m_Camera.SetRadius(m_SavedCamera.Radius);
        m_Camera.SetTarget(m_SavedCamera.Target);
        m_PlayStaticColliders.clear();
        m_PlayerVelocity = { 0.0f, 0.0f, 0.0f };
        m_PlayerOnGround = false;
        m_PlayerBridgeTime = 0.0f;
        m_SkinnedAnimTime = 0.0f;
        m_Projectiles.clear();
        m_BazookaCooldownTimer = 0.0f;
        m_PlayBazookaEquipped = false;
        m_PlayEmoteMenuOpen = false;
        m_PlayMaleEmoteSlotIndex = -1;
        m_PlayMaleEmoteTime = 0.0f;
        m_PlayEmoteDebugTimer = 0.0f;
        s_PlayEmoteLockMovement = false;
        m_Selection.clear();
        m_Selected = -1;

    }

    void BuildPlayStaticColliders(int playerIdx)
    {
        m_PlayStaticColliders.clear();
        for (int i = 0; i < (int)m_Objects.size(); i++) {
            if (i == playerIdx) continue;
            auto& obj = m_Objects[i];
            // Modèles .obj (Custom) : visuel seulement — pas d'AABB par sous-mesh (trop imprécis pour les maps).
            // Collision = uniquement les primitives Cube / Plan placées à la main (voir « Collider invisible »).
            if (obj.Type == PrimitiveType::Custom || obj.Type == PrimitiveType::Folder)
                continue;

            glm::vec3 objCenter = obj.Position;
            if (obj.Type == PrimitiveType::PolarMarker) {
                const float th = glm::radians(obj.PolarThetaDeg);
                objCenter.x = obj.PolarRadius * cosf(th);
                objCenter.z = obj.PolarRadius * sinf(th);
            }

            if (obj.Type == PrimitiveType::Cube || obj.Type == PrimitiveType::Plane || obj.Type == PrimitiveType::PolarMarker) {
                glm::vec3 half = (obj.Type == PrimitiveType::Plane)
                    ? glm::vec3(0.5f * obj.Scale.x, 0.06f * glm::max(1.0f, obj.Scale.y), 0.5f * obj.Scale.z)
                    : glm::vec3(0.5f, 0.5f, 0.5f) * obj.Scale;
                glm::vec3 bmin = objCenter - half;
                glm::vec3 bmax = objCenter + half;
                if (bmin.x > bmax.x) std::swap(bmin.x, bmax.x);
                if (bmin.y > bmax.y) std::swap(bmin.y, bmax.y);
                if (bmin.z > bmax.z) std::swap(bmin.z, bmax.z);
                glm::vec3 sz = bmax - bmin;
                float mh = glm::min(sz.x, sz.z);
                bool isFloorPrim = (sz.y > 0.001f) && (sz.y < mh * m_FloorAspectThreshold);

                if (!isFloorPrim) {
                    glm::vec3 shrink = {
                        glm::min(sz.x * m_ColliderShrinkXZ, 0.35f),
                        0.0f,
                        glm::min(sz.z * m_ColliderShrinkXZ, 0.35f)
                    };
                    bmin += shrink;
                    bmax -= shrink;
                    if (bmin.x >= bmax.x || bmin.y >= bmax.y || bmin.z >= bmax.z)
                        continue;
                }
                else {
                    float topY = bmax.y;
                    bmin.y = topY - glm::min(0.05f, sz.y * 0.5f);
                    bmax.y = topY;
                }

                // "Collider invisible" sert souvent de plateforme de gameplay:
                // on autorise donc l'appui/jump dessus même si la primitive n'est pas classée "floor" par l'aspect ratio.
                bool hasWalkableTop = isFloorPrim || obj.IsColliderOnly;
                m_PlayStaticColliders.push_back({ bmin, bmax, hasWalkableTop });
            }
        }

        int nFloor = 0, nWall = 0;
        for (const auto& c : m_PlayStaticColliders) {
            if (c.IsFloor) nFloor++;
            else           nWall++;
        }
        PURR_CORE_INFO("Colliders built: {} planchers, {} murs (shrink XZ={:.2f})",
            nFloor, nWall, m_ColliderShrinkXZ);
        if (nFloor > 0) {
            float hiY = -FLT_MAX;
            glm::vec3 hiMin(0.0f), hiMax(0.0f);
            for (const auto& c : m_PlayStaticColliders) {
                if (!c.IsFloor) continue;
                if (c.Max.y > hiY) {
                    hiY = c.Max.y;
                    hiMin = c.Min;
                    hiMax = c.Max;
                }
            }
            PURR_CORE_INFO("Plancher le plus haut (AABB): min ({:.3f},{:.3f},{:.3f}) max ({:.3f},{:.3f},{:.3f})",
                hiMin.x, hiMin.y, hiMin.z, hiMax.x, hiMax.y, hiMax.z);
        }
    }

    glm::vec3 ComputeGroundedSpawn(const glm::vec3& desiredSpawn, const glm::vec3& playerScale) const
    {
        float halfHeight = 0.42f * playerScale.y;
        float probeRadius = glm::max(0.12f * playerScale.x, 0.08f);
        bool found = false;
        float bestTop = -FLT_MAX;

        for (const auto& c : m_PlayStaticColliders) {
            if (!c.IsFloor) continue;
            bool overlapsXZ =
                (desiredSpawn.x >= c.Min.x - probeRadius && desiredSpawn.x <= c.Max.x + probeRadius) &&
                (desiredSpawn.z >= c.Min.z - probeRadius && desiredSpawn.z <= c.Max.z + probeRadius);
            if (!overlapsXZ) continue;

            // On ne garde que les planchers sous le spawn (pas plafonds/murs).
            if (c.Max.y > desiredSpawn.y + halfHeight)
                continue;

            // Limite de recherche pour éviter de snap vers des surfaces trop basses.
            if ((desiredSpawn.y - c.Max.y) > m_AutoSpawnSearchHeight)
                continue;

            // On prend le plancher le plus haut sous le spawn.
            if (c.Max.y > bestTop) {
                bestTop = c.Max.y;
                found = true;
            }
        }

        if (found)
            PURR_CORE_INFO("GroundedSpawn: plancher trouve a y={:.3f} -> spawn y={:.3f}", bestTop, bestTop + halfHeight + m_AutoSpawnClearance);
        else
            PURR_CORE_INFO("GroundedSpawn: aucun plancher trouve sous spawn y={:.3f}", desiredSpawn.y);

        if (!found)
            return desiredSpawn;

        glm::vec3 snapped = desiredSpawn;
        snapped.y = bestTop + halfHeight + m_AutoSpawnClearance;
        return snapped;
    }

    bool TryGetSpawnMarkerSpawnPoint(glm::vec3& outSpawnPos) const
    {
        for (const auto& obj : m_Objects) {
            if (obj.Name == "Player")
                continue;

            bool spawnTagged =
                obj.Name.find("Spawn") != std::string::npos ||
                obj.Name.find("spawn") != std::string::npos ||
                obj.Name.find("SPAWN") != std::string::npos;
            if (!spawnTagged)
                continue;

            glm::vec3 pos = obj.GetWorldPosition(m_Objects);
            float markerHalfY = glm::abs(obj.Scale.y) * 0.5f;
            float playerHalfY = glm::abs(m_CurrentPlayerScale.y) * 0.42f;
            outSpawnPos = {
                pos.x,
                pos.y + markerHalfY + playerHalfY + m_AutoSpawnClearance,
                pos.z
            };
            return true;
        }
        return false;
    }

    void ResolvePlayerPhysics(SceneObject& player, float dt)
    {
        glm::vec3 halfExtents = glm::vec3(0.14f, 0.42f, 0.14f) * player.Scale;
        glm::vec3 pos = player.Position;

        // 1) Vertical: gravité + résolution sol/plafond
        m_PlayerVelocity.y += m_Gravity * dt;
        pos.y += m_PlayerVelocity.y * dt;
        m_PlayerOnGround = false;

        for (const auto& c : m_PlayStaticColliders) {
            if (!c.IsFloor) continue;
            bool overlapXZ =
                (pos.x + halfExtents.x > c.Min.x && pos.x - halfExtents.x < c.Max.x) &&
                (pos.z + halfExtents.z > c.Min.z && pos.z - halfExtents.z < c.Max.z);
            if (!overlapXZ) continue;

            float playerBottom = pos.y - halfExtents.y;
            float playerTop = pos.y + halfExtents.y;
            bool overlapY = (playerTop > c.Min.y && playerBottom < c.Max.y);
            if (!overlapY) continue;

            float penDown = playerTop - c.Min.y;      // collision plafond
            float penUp = c.Max.y - playerBottom;     // collision sol

            if (penUp < penDown) {
                pos.y += penUp;
                if (m_PlayerVelocity.y < 0.0f) {
                    m_PlayerVelocity.y = 0.0f;
                    m_PlayerOnGround = true;
                }
            }
            else {
                pos.y -= penDown;
                if (m_PlayerVelocity.y > 0.0f)
                    m_PlayerVelocity.y = 0.0f;
            }
        }

        // 2) Horizontal XZ combiné — un seul axe (pénétration minimale) pour éviter double push au coin d'un cube
        for (const auto& c : m_PlayStaticColliders) {
            bool overlapX = (pos.x + halfExtents.x > c.Min.x && pos.x - halfExtents.x < c.Max.x);
            bool overlapY = (pos.y + halfExtents.y > c.Min.y && pos.y - halfExtents.y < c.Max.y);
            bool overlapZ = (pos.z + halfExtents.z > c.Min.z && pos.z - halfExtents.z < c.Max.z);
            if (!overlapX || !overlapY || !overlapZ) continue;

            float penXL = (pos.x + halfExtents.x) - c.Min.x;
            float penXR = c.Max.x - (pos.x - halfExtents.x);
            float penZB = (pos.z + halfExtents.z) - c.Min.z;
            float penZF = c.Max.z - (pos.z - halfExtents.z);
            float penX = glm::min(penXL, penXR);
            float penZ = glm::min(penZB, penZF);

            if (penX < penZ) {
                if (penXL < penXR) { pos.x -= penXL; if (m_PlayerVelocity.x > 0.0f) m_PlayerVelocity.x = 0.0f; }
                else               { pos.x += penXR; if (m_PlayerVelocity.x < 0.0f) m_PlayerVelocity.x = 0.0f; }
            }
            else {
                if (penZB < penZF) { pos.z -= penZB; if (m_PlayerVelocity.z > 0.0f) m_PlayerVelocity.z = 0.0f; }
                else               { pos.z += penZF; if (m_PlayerVelocity.z < 0.0f) m_PlayerVelocity.z = 0.0f; }
            }
        }

        player.Position = pos;
    }

    void UpdatePlayCamera(const SceneObject& player, float dt)
    {
        const bool fp = IsPlayFirstPerson();
        glm::vec3 base = player.GetWorldPosition(m_Objects);

        if (!fp && m_PlayCameraPrevFirstPerson) {
            m_Camera.SetOrbitAngles(m_PlayFPYawDeg, glm::clamp(m_PlayFPPitchDeg, -45.0f, 80.0f));
        }

        if (fp) {
            // 1P : position type "yeux" + léger décalage vers l'avant ; direction = yaw/pitch (pas orbite au-dessus de la tête).
            float yR = glm::radians(m_PlayFPYawDeg);
            float pR = glm::radians(m_PlayFPPitchDeg);
            glm::vec3 fwd(
                -cosf(yR) * cosf(pR),
                sinf(pR),
                -sinf(yR) * cosf(pR));
            fwd = glm::normalize(fwd);
            glm::vec3 flatFwd(-cosf(yR), 0.0f, -sinf(yR));
            const float flatLenSq = flatFwd.x * flatFwd.x + flatFwd.y * flatFwd.y + flatFwd.z * flatFwd.z;
            if (flatLenSq > 1e-8f)
                flatFwd = glm::normalize(flatFwd);

            // Parametres de camera differents selon l'avatar (male/female).
            const bool isMaleAvatar = (m_PlayerAvatarChoice == PlayerAvatarChoice::Male);
            float eyeHeightScale = isMaleAvatar ? m_PlayFPEyeHeightScaleMale : m_PlayFPEyeHeightScaleFemale;
            float forwardScale = isMaleAvatar ? m_PlayFPEyeForwardScaleMale : m_PlayFPEyeForwardScaleFemale;
            float eyeHeightBase = isMaleAvatar ? m_PlayFPEyeHeightBaseMale : m_PlayFPEyeHeightBaseFemale;
            float forwardBase = isMaleAvatar ? m_PlayFPEyeForwardBaseMale : m_PlayFPEyeForwardBaseFemale;

            // Important: pas de min fixe a 0.18, sinon le male reste "coince" a la meme hauteur visuelle.
            float eyeY = eyeHeightBase + eyeHeightScale * player.Scale.y;
            glm::vec3 eye = base + glm::vec3(0.0f, eyeY, 0.0f) + flatFwd * (forwardBase + forwardScale * player.Scale.y);

            const float R = m_PlayFPSphereRadius;
            glm::vec3 target = eye + fwd * R;
            glm::vec3 v = -fwd;
            float elRad = glm::asin(glm::clamp(v.y, -1.0f, 1.0f));
            float cosEl = cosf(elRad);
            float azRad = (std::fabs(cosEl) > 1e-4f) ? atan2f(v.z, v.x) : 0.0f;

            m_Camera.SetTarget(target);
            m_Camera.SetRadius(R);
            m_Camera.SetOrbitAngles(glm::degrees(azRad), glm::degrees(elRad));
        }
        else {
            glm::vec3 desiredTarget = base + glm::vec3(0.0f, m_PlayCamTargetHeight, 0.0f);
            glm::vec3 smoothed = glm::mix(m_Camera.GetTarget(), desiredTarget, glm::clamp(dt * m_PlayCamFollowSpeed, 0.0f, 1.0f));
            m_Camera.SetTarget(smoothed);
            m_Camera.SetRadius(m_PlayCamDistance);
            m_PlayFPYawDeg = m_Camera.GetAzimuth();
            m_PlayFPPitchDeg = m_Camera.GetElevation();
        }

        m_PlayCameraPrevFirstPerson = fp;
    }

    void ApplyOpacityRecursive(int rootIdx, float opacity)
    {
        if (rootIdx < 0 || rootIdx >= (int)m_Objects.size()) return;
        opacity = glm::clamp(opacity, 0.0f, 1.0f);
        m_Objects[rootIdx].Opacity = opacity;
        for (int j = 0; j < (int)m_Objects.size(); ++j) {
            if (m_Objects[j].ParentIndex == rootIdx)
                ApplyOpacityRecursive(j, opacity);
        }
    }

    static float WrapAngleDelta(float to, float from)
    {
        float d = to - from;
        while (d > 180.0f) d -= 360.0f;
        while (d < -180.0f) d += 360.0f;
        return d;
    }

    static void SortKeyframes(std::vector<KeyframeTRS>& keys)
    {
        std::sort(keys.begin(), keys.end(), [](const KeyframeTRS& a, const KeyframeTRS& b) { return a.Time < b.Time; });
    }

    static float GetAnimationDuration(const std::vector<KeyframeTRS>& keys)
    {
        if (keys.empty()) return 0.0f;
        float d = keys.back().Time - keys.front().Time;
        return glm::max(d, 0.0001f);
    }

    static KeyframeTRS SampleKeys(const std::vector<KeyframeTRS>& keys, float t)
    {
        if (keys.empty()) return {};
        if (keys.size() == 1) return keys.front();
        if (t <= keys.front().Time) return keys.front();
        if (t >= keys.back().Time) return keys.back();

        for (size_t i = 0; i + 1 < keys.size(); ++i) {
            const auto& k0 = keys[i];
            const auto& k1 = keys[i + 1];
            if (t < k0.Time || t > k1.Time) continue;
            float span = glm::max(k1.Time - k0.Time, 0.0001f);
            float a = glm::clamp((t - k0.Time) / span, 0.0f, 1.0f);
            KeyframeTRS out;
            out.Time = t;
            out.Position = glm::mix(k0.Position, k1.Position, a);
            out.Scale = glm::mix(k0.Scale, k1.Scale, a);
            out.Rotation.x = k0.Rotation.x + WrapAngleDelta(k1.Rotation.x, k0.Rotation.x) * a;
            out.Rotation.y = k0.Rotation.y + WrapAngleDelta(k1.Rotation.y, k0.Rotation.y) * a;
            out.Rotation.z = k0.Rotation.z + WrapAngleDelta(k1.Rotation.z, k0.Rotation.z) * a;
            return out;
        }
        return keys.back();
    }

    void UpdateClassicAnimations(float dt)
    {
        for (auto& obj : m_Objects) {
            if (!obj.HasAnimation || !obj.AnimationPlaying || obj.AnimationKeys.size() < 2)
                continue;

            SortKeyframes(obj.AnimationKeys);
            float startT = obj.AnimationKeys.front().Time;
            float endT = obj.AnimationKeys.back().Time;
            float duration = GetAnimationDuration(obj.AnimationKeys);
            obj.AnimationTime += dt * glm::max(0.01f, obj.AnimationSpeed);

            if (obj.AnimationLoop) {
                float rel = obj.AnimationTime - startT;
                while (rel > duration) rel -= duration;
                while (rel < 0.0f) rel += duration;
                obj.AnimationTime = startT + rel;
            }
            else {
                obj.AnimationTime = glm::clamp(obj.AnimationTime, startT, endT);
            }

            KeyframeTRS s = SampleKeys(obj.AnimationKeys, obj.AnimationTime);
            obj.Position = s.Position;
            obj.Rotation = s.Rotation;
            obj.Scale = glm::max(s.Scale, glm::vec3(0.001f));
            if (obj.Type == PrimitiveType::PolarMarker)
                obj.PolarSyncPolarFromCartesianXZ();
        }
    }

    void UpdateSpringAnimations(float dt)
    {
        float safeDt = glm::min(dt, 0.033f);
        for (auto& obj : m_Objects) {
            if (!obj.UseSpring)
                continue;

            glm::vec3 disp = obj.Position - obj.SpringAnchor;
            float invMass = 1.0f / glm::max(0.01f, obj.SpringMass);
            glm::vec3 force = -obj.SpringK * disp - obj.SpringDamping * obj.SpringVelocity;
            glm::vec3 acc = force * invMass;
            obj.SpringVelocity += acc * safeDt;
            obj.Position += obj.SpringVelocity * safeDt;
            if (obj.Type == PrimitiveType::PolarMarker)
                obj.PolarSyncPolarFromCartesianXZ();
        }
    }

    void EnsurePlayerAnimKeys(SceneObject& player)
    {
        if (m_PlayerIdleKeys.empty()) {
            KeyframeTRS a; a.Time = 0.0f; a.Scale = { 1.0f,1.0f,1.0f };
            KeyframeTRS b; b.Time = 0.5f; b.Scale = { 1.0f,1.03f,1.0f };
            KeyframeTRS c; c.Time = 1.0f; c.Scale = { 1.0f,1.0f,1.0f };
            m_PlayerIdleKeys = { a,b,c };
        }
        if (m_PlayerWalkKeys.empty()) {
            KeyframeTRS a; a.Time = 0.0f; a.Scale = { 1.0f,1.0f,1.0f }; a.Rotation = { 0,0,-2.5f };
            KeyframeTRS b; b.Time = 0.2f; b.Scale = { 1.0f,1.06f,1.0f }; b.Rotation = { 0,0,2.5f };
            KeyframeTRS c; c.Time = 0.4f; c.Scale = { 1.0f,1.0f,1.0f }; c.Rotation = { 0,0,-2.5f };
            m_PlayerWalkKeys = { a,b,c };
        }

        player.HasAnimation = true;
        player.AnimationPlaying = true;
        player.AnimationLoop = true;
    }

    void EnsurePlayerAirKeys()
    {
        if (m_PlayerJumpKeys.empty()) {
            KeyframeTRS a; a.Time = 0.0f; a.Scale = { 1.0f, 0.92f, 1.0f }; a.Rotation = { -8.0f, 0.0f, 0.0f };
            KeyframeTRS b = a; b.Time = 0.3f; b.Scale = { 1.0f, 0.98f, 1.0f }; b.Rotation = { -4.0f, 0.0f, 0.0f };
            m_PlayerJumpKeys = { a,b };
        }
        if (m_PlayerFallKeys.empty()) {
            KeyframeTRS a; a.Time = 0.0f; a.Scale = { 1.0f, 1.08f, 1.0f }; a.Rotation = { 10.0f, 0.0f, 0.0f };
            KeyframeTRS b = a; b.Time = 0.3f; b.Scale = { 1.0f, 1.04f, 1.0f }; b.Rotation = { 6.0f, 0.0f, 0.0f };
            m_PlayerFallKeys = { a,b };
        }
    }

    void UpdatePlayerBridgeAnimation(SceneObject& player, float dt)
    {
        EnsurePlayerAnimKeys(player);
        EnsurePlayerAirKeys();
        float horizSpeed = sqrtf(m_PlayerVelocity.x * m_PlayerVelocity.x + m_PlayerVelocity.z * m_PlayerVelocity.z);
        PlayerAnimState newState = PlayerAnimState::Idle;
        if (!m_PlayerOnGround)
            newState = (m_PlayerVelocity.y > 0.05f) ? PlayerAnimState::Jump : PlayerAnimState::Fall;

        if (newState != m_PlayerAnimState) {
            m_PlayerAnimState = newState;
            m_PlayerBridgeTime = 0.0f;
        }

        const std::vector<KeyframeTRS>* keysPtr = &m_PlayerIdleKeys;
        float speedMul = 1.2f;
        if (m_PlayerAnimState == PlayerAnimState::Jump) { keysPtr = &m_PlayerJumpKeys; speedMul = 1.0f; }
        else if (m_PlayerAnimState == PlayerAnimState::Fall) { keysPtr = &m_PlayerFallKeys; speedMul = 1.0f; }

        // Pas d'animation idle: on garde une pose neutre.
        if (m_PlayerAnimState == PlayerAnimState::Idle) {
            player.Scale = GetCurrentPlayerBaseScale();
            player.Rotation.x = 0.0f;
            player.Rotation.z = 0.0f;
            return;
        }

        const std::vector<KeyframeTRS>& keys = *keysPtr;
        if (keys.size() < 2) return;

        m_PlayerBridgeTime += dt * speedMul;
        float startT = keys.front().Time;
        float duration = GetAnimationDuration(keys);
        float rel = m_PlayerBridgeTime - startT;
        while (rel > duration) rel -= duration;
        while (rel < 0.0f) rel += duration;
        float t = startT + rel;
        KeyframeTRS s = SampleKeys(keys, t);

        // Garder un collider stable: pas de scale anime sur le player gameplay.
        // (sinon la physique change apres jump/fall et rend le deplacement bizarre)
        player.Scale = GetCurrentPlayerBaseScale();
        player.Rotation.x = s.Rotation.x;
        player.Rotation.z = s.Rotation.z;
    }

    void UpdateEquippedBazookaAnimation(float dt, const SceneObject& player)
    {
        int bazIdx = FindObjectByName("PlayerBazooka");
        if (bazIdx < 0 || bazIdx >= (int)m_Objects.size())
            return;
        SceneObject& baz = m_Objects[bazIdx];

        // Pas d'idle sway: l'arme reste stable quand le joueur ne bouge pas.
        const float horizSpeed = sqrtf(m_PlayerVelocity.x * m_PlayerVelocity.x + m_PlayerVelocity.z * m_PlayerVelocity.z);
        const bool moving = (horizSpeed > 0.08f && m_PlayerOnGround);
        const float freq = moving ? 10.0f : 0.0f;
        const float ampPos = moving ? 0.020f : 0.0f;
        const float ampRot = moving ? 5.0f : 0.0f;

        m_BazookaAnimTime += dt * freq;
        const float s = sinf(m_BazookaAnimTime);
        const float c = cosf(m_BazookaAnimTime * 0.5f);

        const bool isMaleAvatar = (m_PlayerAvatarChoice == PlayerAvatarChoice::Male);
        const glm::vec3 parentScale = glm::max(glm::abs(player.Scale), glm::vec3(0.0001f));
        const glm::vec3 invParentScale = glm::vec3(1.0f / parentScale.x, 1.0f / parentScale.y, 1.0f / parentScale.z);
        // Offset desire en monde, converti en local pour rester cohérent meme avec un parent tres petit (male).
        const glm::vec3 desiredWorldPos = isMaleAvatar
            ? glm::vec3(0.16f, 0.19f, -0.08f)
            : glm::vec3(0.12f, 0.17f, -0.06f);
        glm::vec3 basePos = desiredWorldPos * invParentScale;

        baz.Position = basePos + glm::vec3(0.0f, ampPos * s, ampPos * 0.5f * c);
        baz.Rotation = { ampRot * 0.35f * s, -90.0f + ampRot * c, ampRot * 0.6f * s };
    }

    int FindObjectByName(const std::string& name)
    {
        for (int i = 0; i < (int)m_Objects.size(); ++i)
            if (m_Objects[i].Name == name) return i;
        return -1;
    }

    void SpawnBazookaForPlayer(int playerIdx)
    {
        if (playerIdx < 0 || playerIdx >= (int)m_Objects.size()) return;
        if (FindObjectByName("PlayerBazooka") >= 0) return;
        const SceneObject& player = m_Objects[playerIdx];

        SceneObject baz;
        baz.Name = "PlayerBazooka";
        baz.Type = PrimitiveType::Custom;
        baz.MeshPath = "assets/models/roblox/bazooka/bazooka1Tex.obj";
        baz.Mat.Diffuse = { 1.0f, 1.0f, 1.0f };
        baz.Mat.Specular = { 0.35f, 0.35f, 0.35f };
        baz.Mat.Shininess = 24.0f;
        baz.Mat.Model = IlluminationModel::Phong;
        baz.ParentIndex = playerIdx;
        const bool isMaleAvatar = (m_PlayerAvatarChoice == PlayerAvatarChoice::Male);
        const glm::vec3 parentScale = glm::max(glm::abs(player.Scale), glm::vec3(0.0001f));
        const glm::vec3 invParentScale = glm::vec3(1.0f / parentScale.x, 1.0f / parentScale.y, 1.0f / parentScale.z);
        const glm::vec3 desiredWorldPos = isMaleAvatar
            ? glm::vec3(0.16f, 0.19f, -0.08f)
            : glm::vec3(0.12f, 0.17f, -0.06f);
        baz.Position = desiredWorldPos * invParentScale;
        baz.Rotation = { 0.0f, -90.0f, 0.0f };
        // Scale desire en monde = 1,1,1 meme avec parent scale tres petit.
        baz.Scale = invParentScale;
        m_Objects.push_back(baz);
        GetMeshForObject(m_Objects.back());
    }

    void FireBazookaProjectile(const SceneObject& player)
    {
        Projectile p;
        float yR = glm::radians(m_PlayFPYawDeg);
        float pR = glm::radians(m_PlayFPPitchDeg);
        glm::vec3 fwd(-cosf(yR) * cosf(pR), sinf(pR), -sinf(yR) * cosf(pR));
        if (glm::length(fwd) < 1e-4f) fwd = { 0,0,-1 };
        fwd = glm::normalize(fwd);
        p.Velocity = fwd * m_ProjectileSpeed;
        p.Position = player.Position + glm::vec3(0.0f, 0.62f * player.Scale.y, 0.0f) + fwd * (0.28f * player.Scale.y);
        p.Life = m_ProjectileLifetime;
        p.Radius = 0.10f * glm::max(player.Scale.y, 0.2f);
        p.Active = true;
        m_Projectiles.push_back(p);
        m_BazookaCooldownTimer = m_BazookaCooldown;
    }

    void ExplodeAt(const glm::vec3& center)
    {
        for (auto& obj : m_Objects) {
            if (obj.Name == "Player" || obj.Name == "PlayerBazooka")
                continue;
            glm::vec3 wp = obj.GetWorldPosition(m_Objects);
            glm::vec3 d = wp - center;
            float len = glm::length(d);
            if (len > m_ExplosionRadius || len < 1e-4f)
                continue;
            glm::vec3 dir = d / len;
            float t = 1.0f - (len / m_ExplosionRadius);
            glm::vec3 impulse = dir * (m_ExplosionForce * t);
            if (obj.UseSpring)
                obj.SpringVelocity += impulse / glm::max(0.05f, obj.SpringMass);
            else
                obj.Position += impulse * 0.03f;
            if (obj.Type == PrimitiveType::PolarMarker)
                obj.PolarSyncPolarFromCartesianXZ();
        }
    }

    void UpdateProjectiles(float dt)
    {
        float safeDt = glm::min(dt, 0.033f);
        for (auto& p : m_Projectiles) {
            if (!p.Active) continue;
            p.Life -= safeDt;
            if (p.Life <= 0.0f) { p.Active = false; continue; }
            p.Velocity.y += m_ProjectileGravity * safeDt;
            p.Position += p.Velocity * safeDt;

            bool hit = false;
            for (const auto& c : m_PlayStaticColliders) {
                bool inside =
                    (p.Position.x >= c.Min.x && p.Position.x <= c.Max.x) &&
                    (p.Position.y >= c.Min.y && p.Position.y <= c.Max.y) &&
                    (p.Position.z >= c.Min.z && p.Position.z <= c.Max.z);
                if (inside) { hit = true; break; }
            }
            if (hit) {
                ExplodeAt(p.Position);
                p.Active = false;
            }
        }
        m_Projectiles.erase(
            std::remove_if(m_Projectiles.begin(), m_Projectiles.end(),
                [](const Projectile& p) { return !p.Active; }),
            m_Projectiles.end());
    }

    void EnsureAuxFramebuffer(std::shared_ptr<Purr::Framebuffer>& fb, const glm::vec2& sz)
    {
        if (sz.x <= 2.0f || sz.y <= 2.0f)
            return;
        uint32_t w = (uint32_t)sz.x, h = (uint32_t)sz.y;
        if (!fb) {
            Purr::FramebufferSpec spec;
            spec.Width = w;
            spec.Height = h;
            fb = std::make_shared<Purr::Framebuffer>(spec);
            return;
        }
        auto& spec = fb->GetSpec();
        if (spec.Width != w || spec.Height != h)
            fb->Resize(w, h);
    }

    void RenderSceneToFramebuffer(Purr::Camera& cam, const std::shared_ptr<Purr::Framebuffer>& target, bool drawEditorOverlays)
    {
        if (!target) return;
        target->Bind();
        Purr::RenderCommand::SetClearColor(0.12f, 0.12f, 0.12f, 1.0f);
        Purr::RenderCommand::Clear();

        glm::mat4 invView = glm::inverse(cam.GetViewMatrix());
        glm::vec3 camPos = glm::vec3(invView[3][0], invView[3][1], invView[3][2]);

        auto uploadLights = [&](std::shared_ptr<Purr::Shader>& sh) {
            sh->SetMat4("u_VP", cam.GetViewProjection());
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

        auto drawObjectAtIndex = [&](int objIdx) {
            SceneObject& obj = m_Objects[objIdx];
            if (m_State == EngineState::Playing && obj.IsColliderOnly)
                return;
            glm::mat4 model = obj.GetWorldTransform(m_Objects);
            glm::mat4 normalMat = glm::transpose(glm::inverse(model));
            auto va = GetMeshForObject(obj);
            if (!va) return;
            const float op = glm::clamp(obj.Opacity, 0.0f, 1.0f);
            AnimationPoseResult skinPose;
            bool hasSkinPose = false;
            auto assetIt = m_AnimatedAssetCache.find(obj.MeshPath);
            if (assetIt != m_AnimatedAssetCache.end()) {
                const bool isMalePlayer = (obj.Name == "Player" && m_PlayerAvatarChoice == PlayerAvatarChoice::Male);
                if (isMalePlayer && m_PlayMaleEmoteSlotIndex >= 0 && m_PlayMaleEmoteSlotIndex < kMaleEmoteSlotCount
                    && m_MaleEmoteClipLoaded[m_PlayMaleEmoteSlotIndex]) {
                    skinPose = EvaluateAnimationPose(
                        assetIt->second, m_MaleEmoteClips[m_PlayMaleEmoteSlotIndex], m_PlayMaleEmoteTime);
                    hasSkinPose = true;
                }
                else if (!assetIt->second.Animations.empty()) {
                    skinPose = EvaluateAnimationPose(assetIt->second, assetIt->second.Animations.front(), m_SkinnedAnimTime);
                    hasSkinPose = true;
                }
            }
            auto drawMeshWithMaterial = [&](const std::shared_ptr<Purr::VertexArray>& mesh, const std::shared_ptr<Purr::Texture>& tex, const glm::vec3& diffuseTint, const std::vector<std::string>& boneNames, const std::string& animNodeName, const glm::mat4& animNodeBindGlobal) {
                glm::vec3 finalDiffuse = obj.Mat.Diffuse * diffuseTint;
                glm::mat4 drawModel = model;
                if (hasSkinPose && boneNames.empty() && !animNodeName.empty()) {
                    auto itG = skinPose.GlobalNodeTransformsByName.find(animNodeName);
                    if (itG != skinPose.GlobalNodeTransformsByName.end()) {
                        glm::mat4 delta = glm::inverse(animNodeBindGlobal) * itG->second;
                        drawModel = model * delta;
                    }
                }
                glm::mat4 drawNormalMat = glm::transpose(glm::inverse(drawModel));
                if (hasSkinPose && !boneNames.empty()) {
                    auto& sh = m_TexSkinShader;
                    sh->Bind();
                    uploadLights(sh);
                    sh->SetMat4("u_Model", drawModel);
                    sh->SetMat4("u_NormalMat", drawNormalMat);
                    sh->SetFloat3("u_MatDiffuse", finalDiffuse);
                    sh->SetFloat3("u_MatSpecular", obj.Mat.Specular);
                    sh->SetFloat("u_MatShininess", obj.Mat.Shininess);
                    sh->SetFloat("u_TilingFactor", obj.TexTiling);
                    sh->SetInt("u_IllumModel", (int)obj.Mat.Model);
                    sh->SetFloat("u_Opacity", op);
                    sh->SetInt("u_UseProceduralTex", obj.UseProceduralTexture ? 1 : 0);
                    sh->SetFloat("u_ProceduralTexScale", obj.ProceduralTexScale);
                    sh->SetInt("u_Texture", 0);
                    for (int bi = 0; bi < 100; ++bi)
                        sh->SetMat4("u_BoneMatrices[" + std::to_string(bi) + "]", glm::mat4(1.0f));
                    for (size_t bi = 0; bi < boneNames.size() && bi < 100; ++bi) {
                        auto itBone = skinPose.FinalBoneMatricesByName.find(boneNames[bi]);
                        if (itBone != skinPose.FinalBoneMatricesByName.end())
                            sh->SetMat4("u_BoneMatrices[" + std::to_string((int)bi) + "]", itBone->second);
                    }
                    if (tex) tex->Bind(0);
                    else if (m_CheckerTex) m_CheckerTex->Bind(0);
                    Purr::RenderCommand::DrawIndexed(mesh);
                }
                else if (tex || obj.UseProceduralTexture) {
                    bool useSkinnedShader = hasSkinPose && !boneNames.empty();
                    auto& sh = useSkinnedShader ? m_TexSkinShader : m_TexShader;
                    sh->Bind();
                    uploadLights(sh);
                    sh->SetMat4("u_Model", drawModel);
                    sh->SetMat4("u_NormalMat", drawNormalMat);
                    sh->SetFloat3("u_MatDiffuse", finalDiffuse);
                    sh->SetFloat3("u_MatSpecular", obj.Mat.Specular);
                    sh->SetFloat("u_MatShininess", obj.Mat.Shininess);
                    sh->SetFloat("u_TilingFactor", obj.TexTiling);
                    sh->SetInt("u_IllumModel", (int)obj.Mat.Model);
                    sh->SetFloat("u_Opacity", op);
                    sh->SetInt("u_UseProceduralTex", obj.UseProceduralTexture ? 1 : 0);
                    sh->SetFloat("u_ProceduralTexScale", obj.ProceduralTexScale);
                    sh->SetInt("u_Texture", 0);
                    if (useSkinnedShader) {
                        for (int bi = 0; bi < 100; ++bi)
                            sh->SetMat4("u_BoneMatrices[" + std::to_string(bi) + "]", glm::mat4(1.0f));
                        for (size_t bi = 0; bi < boneNames.size() && bi < 100; ++bi) {
                            auto itBone = skinPose.FinalBoneMatricesByName.find(boneNames[bi]);
                            if (itBone != skinPose.FinalBoneMatricesByName.end())
                                sh->SetMat4("u_BoneMatrices[" + std::to_string((int)bi) + "]", itBone->second);
                        }
                    }
                    if (tex) tex->Bind(0);
                    else if (m_CheckerTex) m_CheckerTex->Bind(0);
                    Purr::RenderCommand::DrawIndexed(mesh);
                }
                else {
                    m_Shader->Bind();
                    uploadLights(m_Shader);
                    m_Shader->SetMat4("u_Model", drawModel);
                    m_Shader->SetMat4("u_NormalMat", drawNormalMat);
                    m_Shader->SetFloat3("u_MatAmbient", obj.Mat.Ambient);
                    m_Shader->SetFloat3("u_MatDiffuse", finalDiffuse);
                    m_Shader->SetFloat3("u_MatSpecular", obj.Mat.Specular);
                    m_Shader->SetFloat("u_MatShininess", obj.Mat.Shininess);
                    m_Shader->SetInt("u_IllumModel", (int)obj.Mat.Model);
                    m_Shader->SetFloat("u_Opacity", op);
                    Purr::RenderCommand::DrawIndexed(mesh);
                }
            };
            if (obj.Type == PrimitiveType::Custom && !obj.Parts.empty()) {
                for (auto& part : obj.Parts)
                    if (part.Mesh) drawMeshWithMaterial(part.Mesh, part.Texture, part.DiffuseTint, part.BoneNames, part.AnimNodeName, part.AnimNodeBindGlobal);
            }
            else drawMeshWithMaterial(va, obj.Tex, glm::vec3(1.0f), {}, "", glm::mat4(1.0f));
        };

        struct TransparentDraw { int Idx = 0; float Dist = 0.0f; };
        std::vector<TransparentDraw> transparent;
        transparent.reserve(m_Objects.size());
        for (int i = 0; i < (int)m_Objects.size(); i++) {
            const SceneObject& o = m_Objects[i];
            if (m_State == EngineState::Playing && o.IsColliderOnly) continue;
            const float op = glm::clamp(o.Opacity, 0.0f, 1.0f);
            if (op < 0.999f && op > 1e-4f) {
                glm::vec3 wp = o.GetWorldPosition(m_Objects);
                transparent.push_back({ i, glm::length(wp - camPos) });
            }
        }
        std::sort(transparent.begin(), transparent.end(), [](const TransparentDraw& a, const TransparentDraw& b) { return a.Dist > b.Dist; });

        Purr::RenderCommand::SetDepthWrite(true);
        for (int i = 0; i < (int)m_Objects.size(); i++) {
            const SceneObject& o = m_Objects[i];
            if (m_State == EngineState::Playing && o.IsColliderOnly) continue;
            const float op = glm::clamp(o.Opacity, 0.0f, 1.0f);
            if (op < 0.999f) continue;
            drawObjectAtIndex(i);
        }
        if (!transparent.empty()) {
            Purr::RenderCommand::SetBlend(true);
            Purr::RenderCommand::SetDepthWrite(false);
            for (const auto& t : transparent) drawObjectAtIndex(t.Idx);
            Purr::RenderCommand::SetDepthWrite(true);
            Purr::RenderCommand::SetBlend(false);
        }

        if (!m_Projectiles.empty()) {
            m_Shader->Bind();
            uploadLights(m_Shader);
            m_Shader->SetInt("u_IllumModel", (int)IlluminationModel::Phong);
            m_Shader->SetFloat3("u_MatAmbient", glm::vec3(0.3f, 0.2f, 0.05f));
            m_Shader->SetFloat3("u_MatDiffuse", glm::vec3(1.0f, 0.55f, 0.12f));
            m_Shader->SetFloat3("u_MatSpecular", glm::vec3(0.9f, 0.8f, 0.45f));
            m_Shader->SetFloat("u_MatShininess", 48.0f);
            m_Shader->SetFloat("u_Opacity", 1.0f);
            for (const auto& p : m_Projectiles) {
                if (!p.Active) continue;
                glm::mat4 model = glm::translate(glm::mat4(1.0f), p.Position) * glm::scale(glm::mat4(1.0f), glm::vec3(p.Radius));
                glm::mat4 normalMat = glm::transpose(glm::inverse(model));
                m_Shader->SetMat4("u_Model", model);
                m_Shader->SetMat4("u_NormalMat", normalMat);
                Purr::RenderCommand::DrawIndexed(m_SphereVA);
            }
        }

        if (drawEditorOverlays) {
            m_WireShader->Bind();
            m_WireShader->SetMat4("u_VP", cam.GetViewProjection());
            for (int idx : m_Selection) {
                if (idx < 0 || idx >= (int)m_Objects.size()) continue;
                auto& obj = m_Objects[idx];
                glm::mat4 model = obj.GetWorldTransform(m_Objects);
                model = glm::rotate(model, glm::radians(obj.Rotation.x), { 1,0,0 });
                model = glm::rotate(model, glm::radians(obj.Rotation.y), { 0,1,0 });
                model = glm::rotate(model, glm::radians(obj.Rotation.z), { 0,0,1 });
                model = glm::scale(model, glm::vec3(1.02f));
                m_WireShader->SetMat4("u_Model", model);
                m_BBoxVA->Bind();
                Purr::RenderCommand::DrawLines(m_BBoxVA);
            }
        }
        target->Unbind();
    }


    void DeleteSelection()
    {
        if (m_Selection.empty()) return;
        SaveSnapshot();
        std::vector<int> sorted(m_Selection.begin(), m_Selection.end());
        std::sort(sorted.begin(), sorted.end());

        std::vector<int> oldToNew(m_Objects.size(), -1);
        int write = 0;
        size_t delPos = 0;
        for (int i = 0; i < (int)m_Objects.size(); ++i) {
            if (delPos < sorted.size() && sorted[delPos] == i) {
                ++delPos;
                continue;
            }
            oldToNew[i] = write++;
        }

        std::vector<SceneObject> kept;
        kept.reserve(write);
        for (int i = 0; i < (int)m_Objects.size(); ++i)
            if (oldToNew[i] != -1)
                kept.push_back(m_Objects[i]);

        for (auto& o : kept) {
            if (o.ParentIndex < 0 || o.ParentIndex >= (int)oldToNew.size()) {
                o.ParentIndex = -1;
            }
            else {
                o.ParentIndex = oldToNew[o.ParentIndex];
            }
        }

        m_Objects.swap(kept);
        m_Selection.clear();
        m_Selected = -1;
    }


    void OnAttach() override { Purr::RenderCommand::EnableDepthTest(); }

    void SyncPlayCursorAndImGui()
    {
        ImGuiIO& io = ImGui::GetIO();
        Purr::Window& win = Purr::Application::Get().GetWindow();
        const bool fp = (m_State == EngineState::Playing && IsPlayFirstPerson());
        if (fp) {
            win.SetCursorMode(Purr::CursorMode::Disabled);
            io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        }
        else {
            win.SetCursorMode(Purr::CursorMode::Normal);
            io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
        }
    }

    void RestorePlayCursorAfterPlay()
    {
        ImGuiIO& io = ImGui::GetIO();
        Purr::Application::Get().GetWindow().SetCursorMode(Purr::CursorMode::Normal);
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
    }

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
        SanitizeSelectionState();
        SyncPlayCursorAndImGui();
        UpdateClassicAnimations(dt);
        if (m_State != EngineState::Playing)
            UpdateSpringAnimations(dt);
        if (m_State == EngineState::Editor)
        {

            // Undo / Redo -----  Ctrl+Z/ Ctrl+Y - Raccourcis Clavier
            if (!io.WantTextInput)
            {
                if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) Undo();
                if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) Redo();
                if (ImGui::IsKeyPressed(ImGuiKey_H)) m_GizmoMode = GizmoMode::Hand;
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

            // Déplacement caméra editor (WASD + Q/E) quand le viewport est actif.
            if (m_ViewportHovered && !io.WantTextInput)
            {
                glm::vec3 camMove = { 0.0f, 0.0f, 0.0f };
                float az = glm::radians(m_Camera.GetAzimuth());
                glm::vec3 forward = glm::normalize(glm::vec3(-cosf(az), 0.0f, -sinf(az)));
                glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
                if (ImGui::IsKeyDown(ImGuiKey_W)) camMove += forward;
                if (ImGui::IsKeyDown(ImGuiKey_S)) camMove -= forward;
                if (ImGui::IsKeyDown(ImGuiKey_A)) camMove -= right;
                if (ImGui::IsKeyDown(ImGuiKey_D)) camMove += right;
                if (ImGui::IsKeyDown(ImGuiKey_E)) camMove.y += 1.0f;
                if (ImGui::IsKeyDown(ImGuiKey_Q)) camMove.y -= 1.0f;
                if (glm::length(camMove) > 0.0f) {
                    float speed = glm::max(2.0f, m_Camera.GetRadius() * 1.2f);
                    m_Camera.SetTarget(m_Camera.GetTarget() + glm::normalize(camMove) * speed * dt);
                }
            }

            // Orbite (clic droit)
            if (m_ViewportHovered && ImGui::IsMouseDragging(ImGuiMouseButton_Right))
                m_Camera.Orbit(io.MouseDelta.x * 0.4f, -io.MouseDelta.y * 0.4f);

            // --- Gizmo input (ImGui direct, plus fiable que le systeme d'events) ---
            if (m_GizmoMode != GizmoMode::Hand &&
                m_ViewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && m_Selected >= 0 && !m_GizmoDragging)
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
                        glm::vec3 pos = m_Objects[m_Selected].GetWorldPosition(m_Objects);
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

                    glm::vec3 pos = m_Objects[m_Selected].GetWorldPosition(m_Objects);
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
                        for (int idx : m_Selection) {
                            m_Objects[idx].Position += move;
                            if (m_Objects[idx].Type == PrimitiveType::PolarMarker)
                                m_Objects[idx].PolarSyncPolarFromCartesianXZ();
                        }
                    }
                }
                else if (m_GizmoMode == GizmoMode::Rotate)
                {
                    glm::vec3 pos = m_Objects[m_Selected].GetWorldPosition(m_Objects);
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
                    glm::vec3 pos = m_Objects[m_Selected].GetWorldPosition(m_Objects);

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
            if (m_GizmoMode != GizmoMode::Hand &&
                !m_GizmoDragging && m_ViewportHovered && m_Selected >= 0)
            {
                ImGuiIO& io = ImGui::GetIO();
                glm::vec2 mp = { io.MousePos.x - m_ViewportPos.x, io.MousePos.y - m_ViewportPos.y };
                glm::vec2 ndc = { mp.x / m_ViewportSize.x * 2.0f - 1.0f,
                                  1.0f - mp.y / m_ViewportSize.y * 2.0f };
                m_HoveredAxis = HitTestGizmo(ndc);
            }
            else if (!m_ViewportHovered && !m_GizmoDragging)
                m_HoveredAxis = GizmoAxis::None;


        } // fin du mode Editor *_*
        else if (m_State == EngineState::Playing)
        {
            // 3e personne : orbite au clic droit. 1re personne : pas d'orbite souris ici (regard libre sans clic).
            if (!IsPlayFirstPerson() && m_ViewportHovered && ImGui::IsMouseDragging(ImGuiMouseButton_Right))
                m_Camera.Orbit(io.MouseDelta.x * 0.4f, -io.MouseDelta.y * 0.4f);
            if (!IsPlayFirstPerson())
                s_PlayCameraAzimuthDeg = m_Camera.GetAzimuth();
        }

        // Scripts — seulement en Play mode
        if (m_State == EngineState::Playing)
        {
            m_BazookaCooldownTimer = glm::max(0.0f, m_BazookaCooldownTimer - dt);
            if (m_PlayerAvatarChoice == PlayerAvatarChoice::Male && m_PlayMaleEmoteSlotIndex >= 0)
                m_PlayMaleEmoteTime += dt;
            s_PlayEmoteLockMovement = (m_PlayerAvatarChoice == PlayerAvatarChoice::Male && m_PlayMaleEmoteSlotIndex >= 0);
            if (!io.WantTextInput && m_PlayerAvatarChoice == PlayerAvatarChoice::Male && ImGui::IsKeyPressed(ImGuiKey_E))
                m_PlayEmoteMenuOpen = !m_PlayEmoteMenuOpen;
            if (m_PlayerAvatarChoice == PlayerAvatarChoice::Male && m_PlayMaleEmoteSlotIndex >= 0
                && m_PlayMaleEmoteSlotIndex < kMaleEmoteSlotCount) {
                m_PlayEmoteDebugTimer -= dt;
                if (m_PlayEmoteDebugTimer <= 0.0f) {
                    m_PlayEmoteDebugTimer = 1.0f;
                    int pIdxDbg = FindObjectByName("Player");
                    if (pIdxDbg >= 0 && pIdxDbg < (int)m_Objects.size()) {
                        const std::string& meshPath = m_Objects[pIdxDbg].MeshPath;
                        auto assetItDbg = m_AnimatedAssetCache.find(meshPath);
                        if (assetItDbg != m_AnimatedAssetCache.end()) {
                            const auto& asset = assetItDbg->second;
                            const LoadedAnimationClip& dbgClip = m_MaleEmoteClips[m_PlayMaleEmoteSlotIndex];
                            int matchedChannels = 0;
                            for (const auto& ch : dbgClip.Channels) {
                                for (const auto& n : asset.Nodes) {
                                    if (n.Name == ch.NodeName) { matchedChannels++; break; }
                                }
                            }
                            AnimationPoseResult poseDbg = EvaluateAnimationPose(asset, dbgClip, m_PlayMaleEmoteTime);
                            int matchedBones = 0;
                            int meshesWithAnimNode = 0;
                            std::string firstAnimNodeName;
                            if (!asset.Meshes.empty()) {
                                for (const auto& b : asset.Meshes.front().Bones) {
                                    if (poseDbg.FinalBoneMatricesByName.find(b.Name) != poseDbg.FinalBoneMatricesByName.end())
                                        matchedBones++;
                                }
                                for (const auto& m : asset.Meshes) {
                                    if (!m.SourceNodeName.empty()) {
                                        meshesWithAnimNode++;
                                        if (firstAnimNodeName.empty())
                                            firstAnimNodeName = m.SourceNodeName;
                                    }
                                }
                            }
                            PURR_INFO("Emote dbg: t={:.2f} slot={} label='{}' clip='{}' channels={} matchedChannels={} nodes={} meshes={} meshesWithAnimNode={} firstAnimNode='{}' bones(firstMesh)={} matchedBones={} finalMats={}",
                                m_PlayMaleEmoteTime,
                                m_PlayMaleEmoteSlotIndex,
                                kMaleEmoteSlotDefs[m_PlayMaleEmoteSlotIndex].UiLabel,
                                dbgClip.Name,
                                (int)dbgClip.Channels.size(),
                                matchedChannels,
                                (int)asset.Nodes.size(),
                                (int)asset.Meshes.size(),
                                meshesWithAnimNode,
                                firstAnimNodeName,
                                asset.Meshes.empty() ? 0 : (int)asset.Meshes.front().Bones.size(),
                                matchedBones,
                                (int)poseDbg.FinalBoneMatricesByName.size());
                        }
                        else {
                            PURR_WARN("Emote dbg: player mesh asset not cached for '{}'", meshPath);
                        }
                    }
                }
            }
            // Jump: Space en Play (sortie Play déplacée sur Escape).
            if (!io.WantTextInput && !s_PlayEmoteLockMovement && ImGui::IsKeyPressed(ImGuiKey_Space) && m_PlayerOnGround) {
                m_PlayerVelocity.y = m_JumpVelocity;
                m_PlayerOnGround = false;
            }

            // 1P : mouvement souris = tourner caméra + corps (sans clic), avant CatScript pour que WASD suivent le regard.
            for (auto& obj : m_Objects) {
                if (obj.Name != "Player") continue;
                const bool fp = IsPlayFirstPerson();
                s_PlayCameraFirstPerson = fp;
                if (fp) {
                    // Sens standard FPS : souris droite → regarde à droite (signe opposé à l’ancienne version).
                    const float sens = m_PlayFPMouseSens;
                    m_PlayFPYawDeg += io.MouseDelta.x * sens;
                    m_PlayFPPitchDeg = glm::clamp(
                        m_PlayFPPitchDeg - io.MouseDelta.y * sens,
                        m_PlayFPPitchMinDeg, m_PlayFPPitchMaxDeg);
                    s_PlayCameraAzimuthDeg = m_PlayFPYawDeg;
                    // Mesh Roblox : +180° pour l'alignement visuel avec la caméra.
                    obj.Rotation.y = m_PlayFPYawDeg + 180.0f;
                }
                break;
            }

            float safeDt = glm::min(dt, 0.033f);
            m_SkinnedAnimTime += safeDt;
            for (auto& obj : m_Objects)
                if (obj.Script) obj.Script->OnUpdate(safeDt);

            for (auto& obj : m_Objects) {
                if (obj.Name == "Player") {
                    ResolvePlayerPhysics(obj, safeDt);
                    UpdatePlayerBridgeAnimation(obj, safeDt);
                    UpdatePlayCamera(obj, safeDt);
                    if (m_PlayBazookaEquipped)
                        UpdateEquippedBazookaAnimation(safeDt, obj);
                    if (m_PlayBazookaEquipped &&
                        !io.WantTextInput && m_BazookaCooldownTimer <= 0.0f &&
                        ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
                        (m_ViewportHovered || IsPlayFirstPerson()))
                        FireBazookaProjectile(obj);
                    break;
                }
            }
            UpdateSpringAnimations(safeDt);
            UpdateProjectiles(safeDt);
        }


        // Raccourcis Play:
        // - Space : Play depuis l'éditeur
        // - Escape : quitter le mode Play
        if (!io.WantTextInput && m_State == EngineState::Editor && ImGui::IsKeyPressed(ImGuiKey_Space))
            EnterPlayMode();
        if (!io.WantTextInput && m_State == EngineState::Playing && ImGui::IsKeyPressed(ImGuiKey_Escape))
            StopPlayMode();

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

        auto drawObjectAtIndex = [&](int objIdx) {
            SceneObject& obj = m_Objects[objIdx];
            if (m_State == EngineState::Playing && obj.IsColliderOnly)
                return;

            glm::mat4 model = obj.GetWorldTransform(m_Objects);
            glm::mat4 normalMat = glm::transpose(glm::inverse(model));

            auto va = GetMeshForObject(obj);
            if (!va) return;

            const float op = glm::clamp(obj.Opacity, 0.0f, 1.0f);
            AnimationPoseResult skinPose;
            bool hasSkinPose = false;
            auto assetIt = m_AnimatedAssetCache.find(obj.MeshPath);
            if (assetIt != m_AnimatedAssetCache.end()) {
                const bool isMalePlayer = (obj.Name == "Player" && m_PlayerAvatarChoice == PlayerAvatarChoice::Male);
                if (isMalePlayer && m_PlayMaleEmoteSlotIndex >= 0 && m_PlayMaleEmoteSlotIndex < kMaleEmoteSlotCount
                    && m_MaleEmoteClipLoaded[m_PlayMaleEmoteSlotIndex]) {
                    skinPose = EvaluateAnimationPose(
                        assetIt->second, m_MaleEmoteClips[m_PlayMaleEmoteSlotIndex], m_PlayMaleEmoteTime);
                    hasSkinPose = true;
                }
                else if (!assetIt->second.Animations.empty()) {
                    skinPose = EvaluateAnimationPose(assetIt->second, assetIt->second.Animations.front(), m_SkinnedAnimTime);
                    hasSkinPose = true;
                }
            }

            auto drawMeshWithMaterial = [&](const std::shared_ptr<Purr::VertexArray>& mesh, const std::shared_ptr<Purr::Texture>& tex, const glm::vec3& diffuseTint, const std::vector<std::string>& boneNames, const std::string& animNodeName, const glm::mat4& animNodeBindGlobal) {
                glm::vec3 finalDiffuse = obj.Mat.Diffuse * diffuseTint;
                glm::mat4 drawModel = model;
                if (hasSkinPose && boneNames.empty() && !animNodeName.empty()) {
                    auto itG = skinPose.GlobalNodeTransformsByName.find(animNodeName);
                    if (itG != skinPose.GlobalNodeTransformsByName.end()) {
                        glm::mat4 delta = glm::inverse(animNodeBindGlobal) * itG->second;
                        drawModel = model * delta;
                    }
                }
                glm::mat4 drawNormalMat = glm::transpose(glm::inverse(drawModel));
                if (hasSkinPose && !boneNames.empty()) {
                    auto& sh = m_TexSkinShader;
                    sh->Bind();
                    uploadLights(sh);
                    sh->SetMat4("u_Model", drawModel);
                    sh->SetMat4("u_NormalMat", drawNormalMat);
                    sh->SetFloat3("u_MatDiffuse", finalDiffuse);
                    sh->SetFloat3("u_MatSpecular", obj.Mat.Specular);
                    sh->SetFloat("u_MatShininess", obj.Mat.Shininess);
                    sh->SetFloat("u_TilingFactor", obj.TexTiling);
                    sh->SetInt("u_IllumModel", (int)obj.Mat.Model);
                    sh->SetFloat("u_Opacity", op);
                    sh->SetInt("u_UseProceduralTex", obj.UseProceduralTexture ? 1 : 0);
                    sh->SetFloat("u_ProceduralTexScale", obj.ProceduralTexScale);
                    sh->SetInt("u_Texture", 0);
                    for (int bi = 0; bi < 100; ++bi)
                        sh->SetMat4("u_BoneMatrices[" + std::to_string(bi) + "]", glm::mat4(1.0f));
                    for (size_t bi = 0; bi < boneNames.size() && bi < 100; ++bi) {
                        auto itBone = skinPose.FinalBoneMatricesByName.find(boneNames[bi]);
                        if (itBone != skinPose.FinalBoneMatricesByName.end())
                            sh->SetMat4("u_BoneMatrices[" + std::to_string((int)bi) + "]", itBone->second);
                    }
                    if (tex) tex->Bind(0);
                    else if (m_CheckerTex) m_CheckerTex->Bind(0);
                    Purr::RenderCommand::DrawIndexed(mesh);
                }
                else if (tex || obj.UseProceduralTexture) {
                    auto& sh = m_TexShader;
                    sh->Bind();
                    uploadLights(sh);
                    sh->SetMat4("u_Model", drawModel);
                    sh->SetMat4("u_NormalMat", drawNormalMat);
                    sh->SetFloat3("u_MatDiffuse", finalDiffuse);
                    sh->SetFloat3("u_MatSpecular", obj.Mat.Specular);
                    sh->SetFloat("u_MatShininess", obj.Mat.Shininess);
                    sh->SetFloat("u_TilingFactor", obj.TexTiling);
                    sh->SetInt("u_IllumModel", (int)obj.Mat.Model);
                    sh->SetFloat("u_Opacity", op);
                    sh->SetInt("u_UseProceduralTex", obj.UseProceduralTexture ? 1 : 0);
                    sh->SetFloat("u_ProceduralTexScale", obj.ProceduralTexScale);
                    sh->SetInt("u_Texture", 0);
                    if (tex) tex->Bind(0);
                    else if (m_CheckerTex) m_CheckerTex->Bind(0);
                    Purr::RenderCommand::DrawIndexed(mesh);
                }
                else {
                    m_Shader->Bind();
                    uploadLights(m_Shader);
                    m_Shader->SetMat4("u_Model", drawModel);
                    m_Shader->SetMat4("u_NormalMat", drawNormalMat);
                    m_Shader->SetFloat3("u_MatAmbient", obj.Mat.Ambient);
                    m_Shader->SetFloat3("u_MatDiffuse", finalDiffuse);
                    m_Shader->SetFloat3("u_MatSpecular", obj.Mat.Specular);
                    m_Shader->SetFloat("u_MatShininess", obj.Mat.Shininess);
                    m_Shader->SetInt("u_IllumModel", (int)obj.Mat.Model);
                    m_Shader->SetFloat("u_Opacity", op);
                    Purr::RenderCommand::DrawIndexed(mesh);
                }
            };

            if (obj.Type == PrimitiveType::Custom && !obj.Parts.empty()) {
                for (auto& part : obj.Parts) {
                    if (!part.Mesh)
                        continue;
                    drawMeshWithMaterial(part.Mesh, part.Texture, part.DiffuseTint, part.BoneNames, part.AnimNodeName, part.AnimNodeBindGlobal);
                }
            }
            else {
                drawMeshWithMaterial(va, obj.Tex, glm::vec3(1.0f), {}, "", glm::mat4(1.0f));
            }
        };

        struct TransparentDraw {
            int Idx = 0;
            float Dist = 0.0f;
        };
        std::vector<TransparentDraw> transparent;
        transparent.reserve(m_Objects.size());
        for (int i = 0; i < (int)m_Objects.size(); i++) {
            const SceneObject& o = m_Objects[i];
            if (m_State == EngineState::Playing && o.IsColliderOnly)
                continue;
            const float op = glm::clamp(o.Opacity, 0.0f, 1.0f);
            if (op < 0.999f && op > 1e-4f) {
                glm::vec3 wp = o.GetWorldPosition(m_Objects);
                transparent.push_back({ i, glm::length(wp - camPos) });
            }
        }
        std::sort(transparent.begin(), transparent.end(),
            [](const TransparentDraw& a, const TransparentDraw& b) { return a.Dist > b.Dist; });

        Purr::RenderCommand::SetDepthWrite(true);
        for (int i = 0; i < (int)m_Objects.size(); i++) {
            const SceneObject& o = m_Objects[i];
            if (m_State == EngineState::Playing && o.IsColliderOnly)
                continue;
            const float op = glm::clamp(o.Opacity, 0.0f, 1.0f);
            if (op < 0.999f)
                continue;
            drawObjectAtIndex(i);
        }

        if (!transparent.empty()) {
            Purr::RenderCommand::SetBlend(true);
            Purr::RenderCommand::SetDepthWrite(false);
            for (const auto& t : transparent)
                drawObjectAtIndex(t.Idx);
            Purr::RenderCommand::SetDepthWrite(true);
            Purr::RenderCommand::SetBlend(false);
        }

        // Bounding boxes pour tous les sélectionnés
        m_WireShader->Bind();
        m_WireShader->SetMat4("u_VP", m_Camera.GetViewProjection());
        for (int idx : m_Selection)
        {
            if (idx < 0 || idx >= (int)m_Objects.size()) continue;
            auto& obj = m_Objects[idx];
            glm::mat4 model = obj.GetWorldTransform(m_Objects);
            model = glm::rotate(model, glm::radians(obj.Rotation.x), { 1,0,0 });
            model = glm::rotate(model, glm::radians(obj.Rotation.y), { 0,1,0 });
            model = glm::rotate(model, glm::radians(obj.Rotation.z), { 0,0,1 });
            // Couleur différente pour le pivot vs les autres sélectionnés
            model = glm::scale(model, glm::vec3(1.02f));
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
                        glm::translate(glm::mat4(1.0f), obj.GetWorldPosition(m_Objects)) *
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
                        glm::translate(glm::mat4(1.0f), obj.GetWorldPosition(m_Objects)) *
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
                        glm::translate(glm::mat4(1.0f), obj.GetWorldPosition(m_Objects)) *
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
                        glm::translate(glm::mat4(1.0f), obj.GetWorldPosition(m_Objects)) *
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

        // Rendus simultanés supplémentaires pour sous-vues (agencement multi-fenêtres).
        if (m_ViewLayout != MultiViewLayout::Single) {
            EnsureAuxFramebuffer(m_ViewTopFramebuffer, m_AuxTopSize);
            EnsureAuxFramebuffer(m_ViewFrontFramebuffer, m_AuxFrontSize);
            EnsureAuxFramebuffer(m_ViewRightFramebuffer, m_AuxRightSize);

            glm::vec3 focus = (m_Selected >= 0 && m_Selected < (int)m_Objects.size())
                ? m_Objects[m_Selected].GetWorldPosition(m_Objects)
                : m_Camera.GetTarget();
            float baseRadius = glm::max(4.0f, m_Camera.GetRadius() * 1.15f);

            if (m_ViewLayout == MultiViewLayout::Dual || m_ViewLayout == MultiViewLayout::Quad) {
                if (m_ViewTopFramebuffer && m_AuxTopSize.x > 2.0f && m_AuxTopSize.y > 2.0f) {
                    Purr::Camera camTop = m_Camera;
                    camTop.SetProjectionMode(Purr::ProjectionMode::Orthographic);
                    camTop.SetAspectRatio(m_AuxTopSize.x / m_AuxTopSize.y);
                    camTop.SetTarget(focus);
                    camTop.SetOrbitAngles(-90.0f, -85.0f);
                    camTop.SetRadius(baseRadius);
                    RenderSceneToFramebuffer(camTop, m_ViewTopFramebuffer, false);
                }
            }
            if (m_ViewLayout == MultiViewLayout::Quad) {
                if (m_ViewFrontFramebuffer && m_AuxFrontSize.x > 2.0f && m_AuxFrontSize.y > 2.0f) {
                    Purr::Camera camFront = m_Camera;
                    camFront.SetProjectionMode(Purr::ProjectionMode::Orthographic);
                    camFront.SetAspectRatio(m_AuxFrontSize.x / m_AuxFrontSize.y);
                    camFront.SetTarget(focus);
                    camFront.SetOrbitAngles(-90.0f, 0.0f);
                    camFront.SetRadius(baseRadius);
                    RenderSceneToFramebuffer(camFront, m_ViewFrontFramebuffer, false);
                }
                if (m_ViewRightFramebuffer && m_AuxRightSize.x > 2.0f && m_AuxRightSize.y > 2.0f) {
                    Purr::Camera camRight = m_Camera;
                    camRight.SetProjectionMode(Purr::ProjectionMode::Orthographic);
                    camRight.SetAspectRatio(m_AuxRightSize.x / m_AuxRightSize.y);
                    camRight.SetTarget(focus);
                    camRight.SetOrbitAngles(0.0f, 0.0f);
                    camRight.SetRadius(baseRadius);
                    RenderSceneToFramebuffer(camRight, m_ViewRightFramebuffer, false);
                }
            }
        }
    }

    void OnImGuiRender() override
    {
        SetupDockspace();

        // ---- Viewport -----------------------------------------------------
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGuiWindowFlags viewportFlags = ImGuiWindowFlags_None;
        if (m_ViewportFullscreen) {
            ImGuiViewport* vp = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(vp->WorkPos);
            ImGui::SetNextWindowSize(vp->WorkSize);
            ImGui::SetNextWindowViewport(vp->ID);
            viewportFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking;
        }
        ImGui::Begin("Viewport", nullptr, viewportFlags);
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


        ImGui::SetCursorScreenPos(contentPos);
        ImVec2 size = ImGui::GetContentRegionAvail();

        const char* layoutLabels[] = { "1 vue", "2 vues", "4 vues" };
        int layoutIdx = (int)m_ViewLayout;
        ImGui::SetCursorScreenPos(ImVec2(contentPos.x + 8, contentPos.y + 8));
        ImGui::SetNextItemWidth(90.0f);
        ImGui::Combo("##view_layout", &layoutIdx, layoutLabels, 3);
        m_ViewLayout = (MultiViewLayout)layoutIdx;
        ImGui::SameLine();
        if (ImGui::Button(m_ViewportFullscreen ? "Fenetre" : "Plein ecran"))
            m_ViewportFullscreen = !m_ViewportFullscreen;

        auto drawPanel = [&](const ImVec2& p, const ImVec2& s, uint64_t texId, const char* label) {
            ImGui::SetCursorScreenPos(p);
            ImGui::Image((ImTextureID)texId, s, ImVec2(0, 1), ImVec2(1, 0));
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRect(p, ImVec2(p.x + s.x, p.y + s.y), IM_COL32(255, 255, 255, 80), 0.0f, 0, 1.0f);
            dl->AddText(ImVec2(p.x + 6, p.y + 6), IM_COL32(255, 255, 255, 220), label);
        };

        const float pad = 4.0f;
        ImVec2 viewOrigin = ImVec2(contentPos.x, contentPos.y + 34.0f);
        ImVec2 viewSize = ImVec2(size.x, glm::max(1.0f, size.y - 34.0f));
        m_AuxTopSize = { 0.0f, 0.0f };
        m_AuxFrontSize = { 0.0f, 0.0f };
        m_AuxRightSize = { 0.0f, 0.0f };

        if (m_ViewLayout == MultiViewLayout::Single) {
            m_ViewportPos = { viewOrigin.x, viewOrigin.y };
            m_ViewportSize = { viewSize.x, viewSize.y };
            drawPanel(viewOrigin, viewSize, m_Framebuffer->GetColorAttachmentID(), "Perspective");
        }
        else if (m_ViewLayout == MultiViewLayout::Dual) {
            ImVec2 leftSz = ImVec2((viewSize.x - pad) * 0.62f, viewSize.y);
            ImVec2 rightSz = ImVec2(viewSize.x - leftSz.x - pad, viewSize.y);
            ImVec2 leftPos = viewOrigin;
            ImVec2 rightPos = ImVec2(viewOrigin.x + leftSz.x + pad, viewOrigin.y);
            m_ViewportPos = { leftPos.x, leftPos.y };
            m_ViewportSize = { leftSz.x, leftSz.y };
            m_AuxTopSize = { rightSz.x, rightSz.y };
            drawPanel(leftPos, leftSz, m_Framebuffer->GetColorAttachmentID(), "Perspective");
            uint64_t topTex = m_ViewTopFramebuffer ? m_ViewTopFramebuffer->GetColorAttachmentID() : m_Framebuffer->GetColorAttachmentID();
            drawPanel(rightPos, rightSz, topTex, "Top");
        }
        else {
            ImVec2 cellSz = ImVec2((viewSize.x - pad) * 0.5f, (viewSize.y - pad) * 0.5f);
            ImVec2 p00 = viewOrigin;
            ImVec2 p10 = ImVec2(viewOrigin.x + cellSz.x + pad, viewOrigin.y);
            ImVec2 p01 = ImVec2(viewOrigin.x, viewOrigin.y + cellSz.y + pad);
            ImVec2 p11 = ImVec2(viewOrigin.x + cellSz.x + pad, viewOrigin.y + cellSz.y + pad);
            m_ViewportPos = { p00.x, p00.y };
            m_ViewportSize = { cellSz.x, cellSz.y };
            m_AuxTopSize = { cellSz.x, cellSz.y };
            m_AuxFrontSize = { cellSz.x, cellSz.y };
            m_AuxRightSize = { cellSz.x, cellSz.y };
            drawPanel(p00, cellSz, m_Framebuffer->GetColorAttachmentID(), "Perspective");
            uint64_t topTex = m_ViewTopFramebuffer ? m_ViewTopFramebuffer->GetColorAttachmentID() : m_Framebuffer->GetColorAttachmentID();
            uint64_t frontTex = m_ViewFrontFramebuffer ? m_ViewFrontFramebuffer->GetColorAttachmentID() : m_Framebuffer->GetColorAttachmentID();
            uint64_t rightTex = m_ViewRightFramebuffer ? m_ViewRightFramebuffer->GetColorAttachmentID() : m_Framebuffer->GetColorAttachmentID();
            drawPanel(p10, cellSz, topTex, "Top");
            drawPanel(p01, cellSz, frontTex, "Front");
            drawPanel(p11, cellSz, rightTex, "Right");
        }

        // Réticule 1P (curseur masqué par GLFW_CURSOR_DISABLED) : repère au centre du viewport.
        if (m_State == EngineState::Playing && m_PlayCamDistance <= m_PlayCamFPThreshold) {
            ImVec2 center = ImVec2(m_ViewportPos.x + m_ViewportSize.x * 0.5f, m_ViewportPos.y + m_ViewportSize.y * 0.5f);
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const float h = 5.0f;
            const float th = 2.0f;
            const ImU32 outline = IM_COL32(0, 0, 0, 200);
            const ImU32 fill = IM_COL32(255, 255, 255, 230);
            auto line = [&](ImVec2 a, ImVec2 b, ImU32 c, float w) { dl->AddLine(a, b, c, w); };
            line(ImVec2(center.x - h, center.y), ImVec2(center.x + h, center.y), outline, th + 2.0f);
            line(ImVec2(center.x, center.y - h), ImVec2(center.x, center.y + h), outline, th + 2.0f);
            line(ImVec2(center.x - h, center.y), ImVec2(center.x + h, center.y), fill, th);
            line(ImVec2(center.x, center.y - h), ImVec2(center.x, center.y + h), fill, th);
            dl->AddCircleFilled(center, 1.8f, fill);
        }

        // HUD inventaire style Roblox: slot transparent en bas-centre.
        if (m_State == EngineState::Playing) {
            const float slotW = 150.0f;
            const float slotH = 52.0f;
            const float slotX = m_ViewportPos.x + (m_ViewportSize.x - slotW) * 0.5f;
            const float slotY = m_ViewportPos.y + m_ViewportSize.y - slotH - 14.0f;
            ImGui::SetCursorScreenPos(ImVec2(slotX, slotY));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
            const ImVec4 normal = m_PlayBazookaEquipped ? ImVec4(0.30f, 0.58f, 0.95f, 0.55f) : ImVec4(0.10f, 0.10f, 0.10f, 0.35f);
            ImGui::PushStyleColor(ImGuiCol_Button, normal);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.30f, 0.30f, 0.30f, 0.45f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.35f, 0.35f, 0.55f));
            if (ImGui::Button("Bazooka", ImVec2(slotW, slotH))) {
                m_PlayBazookaEquipped = !m_PlayBazookaEquipped;
                if (m_PlayBazookaEquipped) {
                    int pIdx = FindObjectByName("Player");
                    if (pIdx >= 0) {
                        SpawnBazookaForPlayer(pIdx);
                        if (pIdx >= 0 && pIdx < (int)m_Objects.size() && m_Objects[pIdx].Script)
                            m_Objects[pIdx].Script->Owner = &m_Objects[pIdx];
                    }
                }
            }
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar(2);

            // Menu emote (E) - male uniquement.
            if (m_PlayerAvatarChoice == PlayerAvatarChoice::Male && m_PlayEmoteMenuOpen) {
                const float menuW = 360.0f;
                const float menuH = 128.0f;
                const float menuX = m_ViewportPos.x + (m_ViewportSize.x - menuW) * 0.5f;
                const float menuY = m_ViewportPos.y + m_ViewportSize.y - menuH - 78.0f;
                ImGui::SetCursorScreenPos(ImVec2(menuX, menuY));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.05f, 0.05f, 0.06f, 0.55f));
                ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.8f, 0.8f, 0.9f, 0.35f));
                ImGui::BeginChild("##emote_menu_male", ImVec2(menuW, menuH), true);
                ImGui::Text("Emotes (Male) - [E] ouvrir/fermer");
                ImGui::Separator();
                for (int si = 0; si < kMaleEmoteSlotCount; ++si) {
                    if (si > 0)
                        ImGui::SameLine();
                    if (!m_MaleEmoteClipLoaded[si])
                        ImGui::BeginDisabled();
                    const bool on = (m_PlayMaleEmoteSlotIndex == si);
                    std::string btn = kMaleEmoteSlotDefs[si].UiLabel;
                    if (on)
                        btn += " [ON]";
                    btn += "##emote_slot_";
                    btn += std::to_string(si);
                    if (ImGui::Button(btn.c_str(), ImVec2(108.0f, 34.0f))) {
                        if (m_MaleEmoteClipLoaded[si]) {
                            if (on)
                                m_PlayMaleEmoteSlotIndex = -1;
                            else {
                                m_PlayMaleEmoteSlotIndex = si;
                                m_PlayMaleEmoteTime = 0.0f;
                                m_PlayEmoteDebugTimer = 0.0f;
                            }
                            PURR_INFO("Emote toggle: slot={} activeSlot={}", si, m_PlayMaleEmoteSlotIndex);
                        }
                    }
                    if (!m_MaleEmoteClipLoaded[si])
                        ImGui::EndDisabled();
                }
                for (int si = 0; si < kMaleEmoteSlotCount; ++si) {
                    if (!m_MaleEmoteClipLoaded[si])
                        ImGui::TextDisabled("Introuvable: %s", kMaleEmoteSlotDefs[si].RelPath);
                }
                ImGui::EndChild();
                ImGui::PopStyleColor(2);
                ImGui::PopStyleVar();
            }
        }

        // ---- Toolbar gizmo (gauche, sous la barre des vues) — masquee en Play mode ----
        if (m_State == EngineState::Editor)
        {
            // Evite les clics accidentels sur Hand pres des controles "1 vue / 2 vues / 4 vues".
            ImGui::SetCursorScreenPos(ImVec2(contentPos.x + 8, contentPos.y + 42));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

            ImGui::PushStyleColor(ImGuiCol_Button,
                m_GizmoMode == GizmoMode::Hand ? ImVec4(0.9f, 0.8f, 0.2f, 0.9f) : ImVec4(0.25f, 0.25f, 0.25f, 0.85f));
            if (ImGui::Button("  Hand [H]  ")) m_GizmoMode = GizmoMode::Hand;
            ImGui::PopStyleColor(); ImGui::SameLine(0, 4);

            ImGui::PushStyleColor(ImGuiCol_Button,
                m_GizmoMode == GizmoMode::Translate ? ImVec4(0.9f, 0.4f, 0.7f, 0.9f) : ImVec4(0.25f, 0.25f, 0.25f, 0.85f));
            if (ImGui::Button("  Translate [T]  ")) m_GizmoMode = GizmoMode::Translate;
            ImGui::PopStyleColor(); ImGui::SameLine(0, 4);

            ImGui::PushStyleColor(ImGuiCol_Button,
                m_GizmoMode == GizmoMode::Rotate ? ImVec4(0.9f, 0.4f, 0.7f, 0.9f) : ImVec4(0.25f, 0.25f, 0.25f, 0.85f));
            if (ImGui::Button("  Rotation [R]  ")) m_GizmoMode = GizmoMode::Rotate;
            ImGui::PopStyleColor(); ImGui::SameLine(0, 4);

            ImGui::PushStyleColor(ImGuiCol_Button,
                m_GizmoMode == GizmoMode::Scale ? ImVec4(0.9f, 0.4f, 0.7f, 0.9f) : ImVec4(0.25f, 0.25f, 0.25f, 0.85f));
            if (ImGui::Button("  Scale [S]  ")) m_GizmoMode = GizmoMode::Scale;
            ImGui::PopStyleColor();

            ImGui::PopStyleVar();
        }

        // ---- Bouton Play / Stop (centre haut) ----
        {
            const char* btnLabel = (m_State == EngineState::Editor) ? "  >  Play  " : "  []  Stop  ";
            ImVec4 btnColor = (m_State == EngineState::Editor)
                ? ImVec4(0.2f, 0.75f, 0.3f, 0.92f)
                : ImVec4(0.85f, 0.2f, 0.2f, 0.92f);

            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

            const bool inEditor = (m_State == EngineState::Editor);
            std::shared_ptr<Purr::Texture> logoTex = inEditor ? m_PlayButtonTex : m_StopButtonTex;
            bool useImageButton = (logoTex && logoTex->IsValid());
            float btnW = useImageButton ? 150.0f : 110.0f;
            float btnH = useImageButton ? (btnW * (354.0f / 937.0f)) : 0.0f;
            float cx = contentPos.x + m_ViewportSize.x * 0.5f - btnW * 0.5f;
            ImGui::SetCursorScreenPos(ImVec2(cx, contentPos.y + 8));
            bool clicked = false;
            if (useImageButton) {
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                ImGui::PushStyleColor(ImGuiCol_NavCursor, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                // Même UV que Play_Purr (flip vertical) pour Stop_Purr
                clicked = ImGui::ImageButton(
                    inEditor ? "##play_logo_button" : "##stop_logo_button",
                    (ImTextureID)(uintptr_t)logoTex->GetRendererID(),
                    ImVec2(btnW, btnH),
                    ImVec2(0.0f, 1.0f),
                    ImVec2(1.0f, 0.0f)
                );
                ImGui::PopStyleColor(4);
                ImGui::PopStyleVar(2);
            }
            else {
                ImGui::PushStyleColor(ImGuiCol_Button, btnColor);
                clicked = ImGui::Button(btnLabel, ImVec2(btnW, 0));
                ImGui::PopStyleColor();
            }

            if (clicked) {
                if (m_State == EngineState::Editor) EnterPlayMode();
                else                                StopPlayMode();
            }

            ImGui::PopStyleVar();

            if (m_State == EngineState::Playing) {
                ImGui::SetCursorScreenPos(ImVec2(cx + btnW + 8, contentPos.y + 12));
                ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 0.9f), " * PLAYING  [Esc] = stop");
            }
        }

        ImGui::End();




        ImGui::PopStyleVar();

        // ---- Scene --------------------------------------------------------
        ImGui::Begin("Scene");
        {
            const char* themeLabels[] = { "Sombre + rose / mauve", "Sombre (classique)" };
            if (ImGui::Combo("Theme UI", &m_ImGuiThemeIndex, themeLabels, 2)) {
                Purr::ApplyImGuiTheme(m_ImGuiThemeIndex == 0
                    ? Purr::ImGuiThemeKind::DarkPink
                    : Purr::ImGuiThemeKind::DarkClassic);
            }
            ImGui::Separator();
        }
        ImGui::Text("Animation demo");
        if (ImGui::Button("Generer demo sur selection", ImVec2(220.0f, 0.0f)))
            BuildAnimationDemoOnSelection();
        ImGui::SameLine();
        if (ImGui::Button("Play toutes anims", ImVec2(150.0f, 0.0f))) {
            for (auto& o : m_Objects)
                if (o.HasAnimation && !o.AnimationKeys.empty())
                    o.AnimationPlaying = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Stop toutes anims", ImVec2(150.0f, 0.0f))) {
            for (auto& o : m_Objects)
                if (o.HasAnimation) {
                    o.AnimationPlaying = false;
                    if (!o.AnimationKeys.empty())
                        o.AnimationTime = o.AnimationKeys.front().Time;
                }
        }
        if (ImGui::Button("Supprimer demos", ImVec2(220.0f, 0.0f))) {
            SaveSnapshot();
            std::vector<int> toErase;
            for (int i = 0; i < (int)m_Objects.size(); ++i) {
                if (m_Objects[i].Name.find("AnimChild Demo") != std::string::npos)
                    toErase.push_back(i);
            }
            std::sort(toErase.begin(), toErase.end());
            std::vector<int> oldToNew(m_Objects.size(), -1);
            int write = 0;
            size_t delPos = 0;
            for (int i = 0; i < (int)m_Objects.size(); ++i) {
                if (delPos < toErase.size() && toErase[delPos] == i) {
                    ++delPos;
                    continue;
                }
                oldToNew[i] = write++;
            }
            std::vector<SceneObject> kept;
            kept.reserve(write);
            for (int i = 0; i < (int)m_Objects.size(); ++i)
                if (oldToNew[i] != -1)
                    kept.push_back(m_Objects[i]);
            for (auto& o : kept) {
                if (o.ParentIndex < 0 || o.ParentIndex >= (int)oldToNew.size()) o.ParentIndex = -1;
                else o.ParentIndex = oldToNew[o.ParentIndex];
            }
            m_Objects.swap(kept);
            m_Selection.clear();
            m_Selected = m_Objects.empty() ? -1 : 0;
            if (m_Selected >= 0) m_Selection.insert(m_Selected);
        }
        ImGui::TextDisabled("Selectionne un objet parent, puis 'Generer demo' pour creer parent+enfant animes.");
        ImGui::Separator();

        bool isPersp = (m_Camera.GetProjectionMode() == Purr::ProjectionMode::Perspective);
        if (ImGui::RadioButton("Perspective", isPersp))  m_Camera.SetProjectionMode(Purr::ProjectionMode::Perspective);
        ImGui::SameLine();
        if (ImGui::RadioButton("Orthographic", !isPersp)) m_Camera.SetProjectionMode(Purr::ProjectionMode::Orthographic);
        ImGui::Separator();
        ImGui::Text("Objets (%zu)", m_Objects.size());
        ImGui::Checkbox("Afficher masques (liste)", &m_ShowHiddenInSceneList);
        ImGui::TextDisabled("Masquer un objet : decoche la case a gauche de son nom.");




        // ---- Scene list (hiérarchique) ----
        for (int i = 0; i < (int)m_Objects.size(); i++) {
            if (m_Objects[i].ParentIndex != -1)
                continue;
            if (m_Objects[i].HiddenInHierarchy && !m_ShowHiddenInSceneList)
                continue;
            DrawHierarchyNode(i);
        }

        // Drop sur zone vide = détacher du parent
        ImGui::InvisibleButton("##droprootzone", ImVec2(-1, 20));
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("HIERARCHY_NODE")) {
                int src = *(const int*)p->Data;
                ReparentKeepWorld(src, -1);
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::TextDisabled("Ctrl+Click = multi  |  Double-clic = renommer  |  Drag = reparenter");



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
                { "Sphere",            PrimitiveType::Sphere        },
                { "Dossier",           PrimitiveType::Folder        },
                { "Marqueur (polaire)", PrimitiveType::PolarMarker  },
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
                    else if (e.type == PrimitiveType::PolarMarker) {
                        obj.PolarRadius = 2.0f;
                        obj.PolarThetaDeg = 0.0f;
                        obj.Position = { 0.0f, 0.0f, 0.0f };
                        obj.Scale = { 0.25f, 0.25f, 0.25f };
                        obj.PolarSyncCartesianFromPolar();
                    }
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
            if (m_Selection.size() >= 2) {
                ImGui::SameLine();
                if (ImGui::Button("Grouper sous pivot")) {
                    GroupSelectionUnderPivot();
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip("Le pivot (selection principal) devient parent des autres.");
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

        // ---- Importer OBJ / FBX  ----
        if (ImGui::Button("Importer OBJ")) {
            std::string p = OpenFileDialog("Modeles OBJ\0*.obj\0All Files\0*.*\0");
            if (!p.empty()) {
                SceneObject obj;

                std::string filename = p.substr(p.find_last_of("/\\") + 1);
                std::string stem = filename.substr(0, filename.find_last_of('.'));
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
                    obj.Tex = Purr::TextureManager::Load(texPath);
                }

                SaveSnapshot();
                m_Objects.push_back(obj);
                m_Selected = (int)m_Objects.size() - 1;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Importer FBX")) {
            std::string p = OpenFileDialog("Modeles FBX\0*.fbx\0All Files\0*.*\0");
            if (!p.empty()) {
                ImportFBXGrouped(p);
            }
        }

        ImGui::Separator();
        ImGui::Text("Maps (presets)");
        ImGui::TextDisabled("Charge une map Roblox preconfiguree.");
        ImGui::TextDisabled("Coords. non cartesiennes : + Add > Marqueur (polaire) — repere (r, theta, Y).");
        if (m_State == EngineState::Playing) ImGui::BeginDisabled();

        const MapPreset mapPresets[] = {
            { "MM2", "assets/models/roblox/mm2/mm2map1Tex.obj", {0.0f, 0.0f, 0.0f}, {16.0f, 16.0f, 16.0f}, {0.0f, 0.8f, 0.0f}, {0.5f, 0.5f, 0.5f} },
            { "Island", "assets/models/roblox/island/island1Tex.obj", {0.0f, 0.0f, 0.0f}, {16.0f, 16.0f, 16.0f}, {0.0f, -0.210f, -0.187f}, {0.5f, 0.5f, 0.5f} },
            { "Doomspires", "assets/models/roblox/doomspires/doomspires1Tex.obj", {0.0f, 0.0f, 0.0f}, {18.0f, 18.0f, 18.0f}, {0.0f, 1.2f, 0.0f}, {0.5f, 0.5f, 0.5f} },
        };

        for (const auto& preset : mapPresets) {
            if (ImGui::Button(preset.Label, ImVec2(120.0f, 0.0f))) {
                // MM2 charge la scene authoring (.json) avec colliders invisibles déjà placés.
                if (std::string(preset.Label) == "MM2") {
                    LoadScene("assets/models/roblox/mm2/mm2_map.json");
                    m_CurrentMapName = "MM2";
                    // Valeurs d'origine MM2 (comme avant le raccordement JSON)
                    m_CurrentPlayerScale = { 0.5f, 0.5f, 0.5f };
                    m_CurrentMapSpawn = { 0.0f, 0.8f, 0.0f };
                }
                else if (std::string(preset.Label) == "Island") {
                    LoadScene("assets/models/roblox/island/island_map.json");
                    m_CurrentMapName = "Island";
                    m_CurrentPlayerScale = { 0.25f, 0.25f, 0.25f };
                    m_CurrentMapSpawn = { -3.714f, -4.611f, 2.195f };
                }
                else if (std::string(preset.Label) == "Doomspires") {
                    LoadScene("assets/models/roblox/doomspires/doomspires_map.json");
                    m_CurrentMapName = "Doomspires";
                    m_CurrentPlayerScale = { 0.25f, 0.25f, 0.25f };
                    m_CurrentMapSpawn = { 0.245f, 2.472f, -0.220f };
                }
                else
                    LoadMapPreset(preset);
            }
            ImGui::SameLine();
        }
        ImGui::NewLine();
        ImGui::Text("Avatar joueur");
        bool isMale = (m_PlayerAvatarChoice == PlayerAvatarChoice::Male);
        bool isFemale = (m_PlayerAvatarChoice == PlayerAvatarChoice::Female);
        if (ImGui::RadioButton("M", isMale)) m_PlayerAvatarChoice = PlayerAvatarChoice::Male;
        ImGui::SameLine();
        if (ImGui::RadioButton("F", isFemale)) m_PlayerAvatarChoice = PlayerAvatarChoice::Female;
        ImGui::TextDisabled("M path: %s", m_PlayerMaleMeshPath.c_str());
        ImGui::TextDisabled("F path: %s", m_PlayerFemaleMeshPath.c_str());
        ImGui::Separator();

        if (ImGui::Button("Ajouter Spawn Marker", ImVec2(180.0f, 0.0f))) {
            SceneObject marker;
            marker.Name = "SpawnMarker";
            marker.Type = PrimitiveType::Cube;
            marker.Position = m_CurrentMapSpawn;
            marker.Scale = { 0.6f, 0.12f, 0.6f };
            marker.Mat.Diffuse = { 0.2f, 1.0f, 0.2f };
            SaveSnapshot();
            m_Objects.push_back(marker);
            m_Selected = (int)m_Objects.size() - 1;
            m_Selection.clear();
            m_Selection.insert(m_Selected);
        }
        if (!m_CurrentMapName.empty())
            ImGui::TextColored({ 0.6f, 1.0f, 0.6f, 1.0f }, "Map chargee : %s", m_CurrentMapName.c_str());
        glm::vec3 markerPos = {};
        if (TryGetSpawnMarkerSpawnPoint(markerPos))
            ImGui::TextColored({ 0.6f, 1.0f, 0.6f, 1.0f }, "Spawn marker detecte: %.3f, %.3f, %.3f", markerPos.x, markerPos.y, markerPos.z);
        else
            ImGui::TextDisabled("Aucun SpawnMarker detecte (fallback preset/autosnap).");
        if (!m_CurrentMapName.empty()) {
            ImGui::Text("Spawn joueur (preset)");
            ImGui::DragFloat3("##map_spawn_player", glm::value_ptr(m_CurrentMapSpawn), 0.05f);
            ImGui::Text("Scale joueur (preset)");
            ImGui::DragFloat3("##map_scale_player", glm::value_ptr(m_CurrentPlayerScale), 0.01f, 0.1f, 10.0f);
            ImGui::Separator();
            ImGui::Text("Ajustements avatar");
            ImGui::DragFloat3("Male scale mult", glm::value_ptr(m_MaleAvatarScaleMultiplier), 0.01f, 0.1f, 4.0f);
            ImGui::DragFloat3("Male spawn offset", glm::value_ptr(m_MaleAvatarSpawnOffset), 0.01f, -5.0f, 5.0f);
            ImGui::DragFloat3("Female scale mult", glm::value_ptr(m_FemaleAvatarScaleMultiplier), 0.01f, 0.1f, 4.0f);
            ImGui::DragFloat3("Female spawn offset", glm::value_ptr(m_FemaleAvatarSpawnOffset), 0.01f, -5.0f, 5.0f);
            ImGui::Checkbox("Auto spawn snap au sol", &m_EnableAutoSpawnSnap);
            ImGui::DragFloat("Spawn search height", &m_AutoSpawnSearchHeight, 0.1f, 0.2f, 50.0f);
            ImGui::DragFloat("Spawn clearance", &m_AutoSpawnClearance, 0.01f, 0.0f, 1.0f);
            ImGui::Separator();
            ImGui::Text("Play camera");
            ImGui::DragFloat("Distance", &m_PlayCamDistance, 0.05f, m_PlayCamMinDist, m_PlayCamMaxDist);
            ImGui::DragFloat("Azimuth", &m_PlayCamAzimuth, 0.5f, -180.0f, 180.0f);
            ImGui::DragFloat("Elevation", &m_PlayCamElevation, 0.5f, -45.0f, 80.0f);
            ImGui::DragFloat("Target Height", &m_PlayCamTargetHeight, 0.02f, 0.0f, 4.0f);
            ImGui::DragFloat("Follow Speed", &m_PlayCamFollowSpeed, 0.1f, 1.0f, 30.0f);
            ImGui::Separator();
            ImGui::Text("Play camera 1P (avatar)");
            ImGui::DragFloat("Female eye base", &m_PlayFPEyeHeightBaseFemale, 0.01f, 0.0f, 3.0f);
            ImGui::DragFloat("Female eye scale", &m_PlayFPEyeHeightScaleFemale, 0.01f, 0.0f, 3.0f);
            ImGui::DragFloat("Female forward base", &m_PlayFPEyeForwardBaseFemale, 0.01f, -2.0f, 2.0f);
            ImGui::DragFloat("Female forward scale", &m_PlayFPEyeForwardScaleFemale, 0.01f, -2.0f, 2.0f);
            ImGui::DragFloat("Male eye base", &m_PlayFPEyeHeightBaseMale, 0.01f, 0.0f, 3.0f);
            ImGui::DragFloat("Male eye scale", &m_PlayFPEyeHeightScaleMale, 0.01f, 0.0f, 3.0f);
            ImGui::DragFloat("Male forward base", &m_PlayFPEyeForwardBaseMale, 0.01f, -2.0f, 2.0f);
            ImGui::DragFloat("Male forward scale", &m_PlayFPEyeForwardScaleMale, 0.01f, -2.0f, 2.0f);
            ImGui::Separator();
            ImGui::Text("Collision");
            ImGui::TextDisabled("Les maps .obj n'ont pas de collision auto : ajoute des Cube/Plan.");
            ImGui::SliderFloat("Shrink XZ (murs Cube/Plan)", &m_ColliderShrinkXZ, 0.0f, 0.35f);
            ImGui::SliderFloat("Floor flatness (h / min XZ)", &m_FloorAspectThreshold, 0.05f, 0.5f);
            ImGui::TextDisabled("Utilise au prochain Play.");
            ImGui::Separator();
            ImGui::Text("Bazooka / projectile");
            ImGui::DragFloat("Projectile speed", &m_ProjectileSpeed, 0.2f, 2.0f, 60.0f);
            ImGui::DragFloat("Projectile life", &m_ProjectileLifetime, 0.05f, 0.2f, 10.0f);
            ImGui::DragFloat("Explosion radius", &m_ExplosionRadius, 0.05f, 0.2f, 10.0f);
            ImGui::DragFloat("Explosion force", &m_ExplosionForce, 0.1f, 0.1f, 30.0f);
            ImGui::TextDisabled("Tir: clic gauche en Play.");
        }

        if (m_State == EngineState::Playing) ImGui::EndDisabled();

        if (m_State == EngineState::Playing) {
            for (const auto& obj : m_Objects) {
                if (obj.Name == "Player") {
                    ImGui::Text("Player live pos: %.3f, %.3f, %.3f",
                        obj.Position.x, obj.Position.y, obj.Position.z);
                    break;
                }
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
            if (obj.Type == PrimitiveType::PolarMarker) {
                ImGui::TextDisabled("Position (cylindrique) : rayon r, angle theta, hauteur Y.");
                if (ImGui::DragFloat("Rayon r", &obj.PolarRadius, 0.05f, 0.0f, 500.0f))
                    obj.PolarSyncCartesianFromPolar();
                if (ImGui::IsItemDeactivatedAfterEdit()) SaveSnapshot();
                if (ImGui::DragFloat("Theta (deg)", &obj.PolarThetaDeg, 0.5f, -720.0f, 720.0f))
                    obj.PolarSyncCartesianFromPolar();
                if (ImGui::IsItemDeactivatedAfterEdit()) SaveSnapshot();
                if (ImGui::DragFloat("Hauteur Y", &obj.Position.y, 0.05f))
                    obj.PolarSyncCartesianFromPolar();
                if (ImGui::IsItemDeactivatedAfterEdit()) SaveSnapshot();
                float th = glm::radians(obj.PolarThetaDeg);
                float px = obj.PolarRadius * cosf(th);
                float pz = obj.PolarRadius * sinf(th);
                ImGui::TextDisabled("Equiv. cartesien (XZ derive) : %.3f, %.3f", px, pz);
            }
            else {
                ImGui::DragFloat3("Position", glm::value_ptr(obj.Position), 0.05f);
                if (ImGui::IsItemDeactivatedAfterEdit()) SaveSnapshot();
            }
            ImGui::DragFloat3("Rotation", glm::value_ptr(obj.Rotation), 0.5f);
            if (ImGui::IsItemDeactivatedAfterEdit()) SaveSnapshot();
            ImGui::DragFloat3("Scale", glm::value_ptr(obj.Scale), 0.01f, 0.01f, 10.0f);
            if (ImGui::IsItemDeactivatedAfterEdit()) SaveSnapshot();
            if (ImGui::Checkbox("Collider invisible (jeu)", &obj.IsColliderOnly))
                SaveSnapshot();
            ImGui::SliderFloat("Opacite (vue)", &obj.Opacity, 0.0f, 1.0f, "%.2f");
            if (obj.Type == PrimitiveType::Folder && ImGui::IsItemEdited())
                ApplyOpacityRecursive(m_Selected, glm::clamp(obj.Opacity, 0.0f, 1.0f));
            if (ImGui::IsItemDeactivatedAfterEdit()) SaveSnapshot();
            if (ImGui::Checkbox("Masque dans la liste Scene", &obj.HiddenInHierarchy))
                SaveSnapshot();
            ImGui::Separator();

            ImGui::Text("Animation classique");
            if (ImGui::Checkbox("Activer animation", &obj.HasAnimation))
                SaveSnapshot();
            if (obj.HasAnimation) {
                ImGui::Checkbox("Loop", &obj.AnimationLoop);
                ImGui::DragFloat("Vitesse anim", &obj.AnimationSpeed, 0.05f, 0.05f, 5.0f);
                if (ImGui::Button("Play")) obj.AnimationPlaying = true;
                ImGui::SameLine();
                if (ImGui::Button("Pause")) obj.AnimationPlaying = false;
                ImGui::SameLine();
                if (ImGui::Button("Stop")) {
                    obj.AnimationPlaying = false;
                    obj.AnimationTime = obj.AnimationKeys.empty() ? 0.0f : obj.AnimationKeys.front().Time;
                }
                float animMin = obj.AnimationKeys.empty() ? 0.0f : obj.AnimationKeys.front().Time;
                float animMax = obj.AnimationKeys.empty() ? 2.0f : obj.AnimationKeys.back().Time;
                if (animMax < animMin + 0.001f) animMax = animMin + 2.0f;
                ImGui::SliderFloat("Temps", &obj.AnimationTime, animMin, animMax, "%.2f");
                if (ImGui::Button("Ajouter keyframe (t)")) {
                    SaveSnapshot();
                    KeyframeTRS k;
                    k.Time = obj.AnimationTime;
                    k.Position = obj.Position;
                    k.Rotation = obj.Rotation;
                    k.Scale = obj.Scale;
                    obj.AnimationKeys.push_back(k);
                    SortKeyframes(obj.AnimationKeys);
                }
                if (!obj.AnimationKeys.empty()) {
                    if (ImGui::TreeNode("Keyframes")) {
                        int toDelete = -1;
                        for (int i = 0; i < (int)obj.AnimationKeys.size(); ++i) {
                            ImGui::PushID(i);
                            auto& k = obj.AnimationKeys[i];
                            ImGui::DragFloat("t", &k.Time, 0.02f, 0.0f, 300.0f);
                            ImGui::SameLine();
                            if (ImGui::Button("Suppr")) toDelete = i;
                            ImGui::DragFloat3("Pos", glm::value_ptr(k.Position), 0.02f);
                            ImGui::DragFloat3("Rot", glm::value_ptr(k.Rotation), 0.2f);
                            ImGui::DragFloat3("Scale", glm::value_ptr(k.Scale), 0.02f, 0.001f, 100.0f);
                            ImGui::Separator();
                            ImGui::PopID();
                        }
                        if (ImGui::IsItemDeactivatedAfterEdit()) {
                            SaveSnapshot();
                            SortKeyframes(obj.AnimationKeys);
                        }
                        if (toDelete >= 0) {
                            SaveSnapshot();
                            obj.AnimationKeys.erase(obj.AnimationKeys.begin() + toDelete);
                        }
                        ImGui::TreePop();
                    }
                }
                else {
                    ImGui::TextDisabled("Aucun keyframe");
                }
            }

            ImGui::Separator();
            ImGui::Text("Animation physique (spring)");
            if (ImGui::Checkbox("Activer spring", &obj.UseSpring))
                SaveSnapshot();
            if (obj.UseSpring) {
                ImGui::DragFloat3("Anchor", glm::value_ptr(obj.SpringAnchor), 0.05f);
                ImGui::DragFloat("k", &obj.SpringK, 0.1f, 0.0f, 200.0f);
                ImGui::DragFloat("Damping", &obj.SpringDamping, 0.05f, 0.0f, 60.0f);
                ImGui::DragFloat("Mass", &obj.SpringMass, 0.05f, 0.05f, 50.0f);
                if (ImGui::Button("Anchor = pos")) obj.SpringAnchor = obj.Position;
                ImGui::SameLine();
                if (ImGui::Button("Kick +X")) obj.SpringVelocity += glm::vec3(2.5f, 0.0f, 0.0f);
                if (ImGui::IsItemDeactivatedAfterEdit()) SaveSnapshot();
            }

            ImGui::Separator();
            if (obj.Type != PrimitiveType::Folder) {
                if (obj.Type == PrimitiveType::PolarMarker)
                    ImGui::TextDisabled("Repere cylindrique (r, theta, Y) — transforme monde en cartesien pour le rendu.");
                ImGui::Text("Texture");
                if (obj.Tex && !obj.TexPath.empty()) {
                    std::string fn = obj.TexPath.substr(obj.TexPath.find_last_of("/\\") + 1);
                    ImGui::TextColored({ 0.5f,1.0f,0.5f,1.0f }, "%s", fn.c_str());
                }
                else { ImGui::TextDisabled("Aucune texture"); }
                if (ImGui::Checkbox("Texture procedurale (damier)", &obj.UseProceduralTexture))
                    SaveSnapshot();
                if (obj.UseProceduralTexture) {
                    ImGui::DragFloat("Echelle proc.", &obj.ProceduralTexScale, 0.1f, 1.0f, 64.0f);
                    if (ImGui::IsItemDeactivatedAfterEdit()) SaveSnapshot();
                    ImGui::TextDisabled("Generee par shader, sans fichier image.");
                }
                if (ImGui::Button("Charger texture...")) {
                    //std::string path = OpenFileDialog();
                    std::string path = OpenFileDialog("Images\0*.png;*.jpg;*.jpeg;*.bmp;*.tga\0All Files\0*.*\0");
                    if (!path.empty()) { obj.Tex = Purr::TextureManager::Load(path); obj.TexPath = path; obj.UseProceduralTexture = false; }
                }
                if (obj.Tex) { ImGui::SameLine(); if (ImGui::Button("Retirer")) { obj.Tex = nullptr; obj.TexPath = ""; } }
                ImGui::Separator();
                ImGui::Text("Materiau");
                static const char* presetNames[] = { "Personnalise","Or","Plastique rouge","Caoutchouc" };
                static int presetIdx = 0;
                static constexpr const char* kMatTexGold = "assets/models/materials/gold_texture.jpg";
                static constexpr const char* kMatTexRubber = "assets/models/materials/rubber_texture.jpg";
                if (ImGui::Combo("##preset", &presetIdx, presetNames, 4)) {
                    SaveSnapshot();
                    switch (presetIdx) {
                    case 1:
                        obj.Mat.Ambient = { 0.25f,0.20f,0.07f }; obj.Mat.Diffuse = { 0.75f,0.61f,0.23f }; obj.Mat.Specular = { 0.63f,0.56f,0.37f }; obj.Mat.Shininess = 51.2f;
                        obj.Tex = Purr::TextureManager::Load(kMatTexGold);
                        obj.TexPath = obj.Tex ? std::string(kMatTexGold) : "";
                        break;
                    case 2:
                        obj.Mat.Ambient = { 0.05f,0,0 }; obj.Mat.Diffuse = { 0.5f,0,0 }; obj.Mat.Specular = { 0.7f,0.6f,0.6f }; obj.Mat.Shininess = 32.0f;
                        obj.Tex = nullptr;
                        obj.TexPath = "";
                        break;
                    case 3:
                        obj.Mat.Ambient = { 0.02f,0.02f,0.02f }; obj.Mat.Diffuse = { 0.01f,0.01f,0.01f }; obj.Mat.Specular = { 0.4f,0.4f,0.4f }; obj.Mat.Shininess = 10.0f;
                        obj.Tex = Purr::TextureManager::Load(kMatTexRubber);
                        obj.TexPath = obj.Tex ? std::string(kMatTexRubber) : "";
                        break;
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
            }
            else {
                ImGui::TextDisabled("Dossier : pas de mesh / texture (conteneur hierarchie).");
            }


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




            // ---- Script -----------------------------------------------
            if (m_Selected >= 0 && m_Selected < (int)m_Objects.size()
                && m_Objects[m_Selected].Script)
            {
                auto* script = m_Objects[m_Selected].Script.get();

                ImGui::Separator();
                std::string header = std::string("[>]  ") + script->GetName();
                if (ImGui::CollapsingHeader(header.c_str()))
                {
                    ImVec2 avail = ImGui::GetContentRegionAvail();
                    ImGui::InputTextMultiline(
                        "##src",
                        const_cast<char*>(script->GetSource()),
                        strlen(script->GetSource()) + 1,
                        ImVec2(avail.x, 300.0f),
                        ImGuiInputTextFlags_ReadOnly
                    );
                    ImGui::TextDisabled("Attache a : %s", m_Objects[m_Selected].Name.c_str());
                }
            }

            ImGui::End();
        }
    }

    void OnEvent(Purr::Event& e) override
    {
        Purr::EventDispatcher dispatcher(e);

        // Zoom caméra (éditeur + play). En 1P, on accepte le scroll même sans hover strict du viewport
        // pour permettre de sortir du mode 1P exactement comme Roblox.
        dispatcher.Dispatch<Purr::MouseScrolledEvent>([this](Purr::MouseScrolledEvent& e) {
            if (m_State == EngineState::Playing) {
                if (!(m_ViewportHovered || IsPlayFirstPerson()))
                    return false;
                const float wheelStep = IsPlayFirstPerson() ? 0.55f : 0.35f;
                m_PlayCamDistance = glm::clamp(
                    m_PlayCamDistance - (float)e.GetYOffset() * wheelStep,
                    m_PlayCamMinDist, m_PlayCamMaxDist);
                if (!IsPlayFirstPerson())
                    m_Camera.SetRadius(m_PlayCamDistance);
                return false;
            }
            if (m_ViewportHovered)
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
        SanitizeSelectionState();
    }
    void Redo()
    {
        if (m_RedoStack.empty()) return;
        m_UndoStack.push_back(m_Objects);
        m_Objects = m_RedoStack.back();
        m_RedoStack.pop_back();
        SanitizeSelectionState();
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
            if (obj.Type == PrimitiveType::PolarMarker)
                obj.PolarSyncCartesianFromPolar();
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
            o["colliderOnly"] = obj.IsColliderOnly;
            o["hiddenInList"] = obj.HiddenInHierarchy;
            o["opacity"] = obj.Opacity;
            o["proceduralTex"] = obj.UseProceduralTexture;
            o["proceduralTexScale"] = obj.ProceduralTexScale;
            o["hasAnimation"] = obj.HasAnimation;
            o["animationPlaying"] = obj.AnimationPlaying;
            o["animationLoop"] = obj.AnimationLoop;
            o["animationTime"] = obj.AnimationTime;
            o["animationSpeed"] = obj.AnimationSpeed;
            o["animationKeys"] = json::array();
            for (const auto& k : obj.AnimationKeys) {
                json jk;
                jk["t"] = k.Time;
                jk["pos"] = { k.Position.x, k.Position.y, k.Position.z };
                jk["rot"] = { k.Rotation.x, k.Rotation.y, k.Rotation.z };
                jk["scale"] = { k.Scale.x, k.Scale.y, k.Scale.z };
                o["animationKeys"].push_back(jk);
            }
            o["useSpring"] = obj.UseSpring;
            o["springAnchor"] = { obj.SpringAnchor.x, obj.SpringAnchor.y, obj.SpringAnchor.z };
            o["springVel"] = { obj.SpringVelocity.x, obj.SpringVelocity.y, obj.SpringVelocity.z };
            o["springK"] = obj.SpringK;
            o["springDamping"] = obj.SpringDamping;
            o["springMass"] = obj.SpringMass;
            if (obj.Type == PrimitiveType::PolarMarker) {
                o["polarRadius"] = obj.PolarRadius;
                o["polarThetaDeg"] = obj.PolarThetaDeg;
            }
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
        m_ObjMultiMeshCache.clear();
        m_AnimatedAssetCache.clear();
        m_Selected = m_Objects.empty() ? -1 : 0;

        for (auto& o : j["objects"])
        {
            SceneObject obj;
            obj.Name = o["name"];
            int typeInt = o.value("type", 0);
            typeInt = glm::clamp(typeInt, 0, (int)PrimitiveType::PolarMarker);
            obj.Type = (PrimitiveType)typeInt;
            obj.MeshPath = o.value("meshPath", "");
            obj.TexPath = o.value("texPath", "");
            obj.Position = { o["pos"][0],   o["pos"][1],   o["pos"][2] };
            obj.Rotation = { o["rot"][0],   o["rot"][1],   o["rot"][2] };
            obj.Scale = { o["scale"][0], o["scale"][1], o["scale"][2] };
            obj.Mat.Diffuse = { o["diffuse"][0],  o["diffuse"][1],  o["diffuse"][2] };
            obj.Mat.Specular = { o["specular"][0], o["specular"][1], o["specular"][2] };
            obj.Mat.Shininess = o["shininess"];
            obj.Mat.Model = (IlluminationModel)(int)o["illum"];
            obj.IsColliderOnly = o.value("colliderOnly", false);
            obj.HiddenInHierarchy = o.value("hiddenInList", false);
            obj.Opacity = o.value("opacity", 1.0f);
            obj.UseProceduralTexture = o.value("proceduralTex", false);
            obj.ProceduralTexScale = o.value("proceduralTexScale", 8.0f);
            obj.HasAnimation = o.value("hasAnimation", false);
            obj.AnimationPlaying = o.value("animationPlaying", false);
            obj.AnimationLoop = o.value("animationLoop", true);
            obj.AnimationTime = o.value("animationTime", 0.0f);
            obj.AnimationSpeed = o.value("animationSpeed", 1.0f);
            obj.AnimationKeys.clear();
            if (o.contains("animationKeys") && o["animationKeys"].is_array()) {
                for (auto& jk : o["animationKeys"]) {
                    KeyframeTRS k;
                    k.Time = jk.value("t", 0.0f);
                    if (jk.contains("pos")) k.Position = { jk["pos"][0], jk["pos"][1], jk["pos"][2] };
                    if (jk.contains("rot")) k.Rotation = { jk["rot"][0], jk["rot"][1], jk["rot"][2] };
                    if (jk.contains("scale")) k.Scale = { jk["scale"][0], jk["scale"][1], jk["scale"][2] };
                    obj.AnimationKeys.push_back(k);
                }
                SortKeyframes(obj.AnimationKeys);
            }
            obj.UseSpring = o.value("useSpring", false);
            if (o.contains("springAnchor")) obj.SpringAnchor = { o["springAnchor"][0], o["springAnchor"][1], o["springAnchor"][2] };
            if (o.contains("springVel")) obj.SpringVelocity = { o["springVel"][0], o["springVel"][1], o["springVel"][2] };
            obj.SpringK = o.value("springK", 20.0f);
            obj.SpringDamping = o.value("springDamping", 5.0f);
            obj.SpringMass = o.value("springMass", 1.0f);
            if (obj.Type == PrimitiveType::PolarMarker) {
                if (o.contains("polarRadius")) {
                    obj.PolarRadius = o["polarRadius"].get<float>();
                    obj.PolarThetaDeg = o.value("polarThetaDeg", 0.0f);
                } else {
                    obj.PolarRadius = sqrtf(obj.Position.x * obj.Position.x + obj.Position.z * obj.Position.z);
                    obj.PolarThetaDeg = glm::degrees(atan2f(obj.Position.z, obj.Position.x));
                }
                obj.PolarSyncCartesianFromPolar();
            }
            else {
                obj.PolarRadius = o.value("polarRadius", 0.0f);
                obj.PolarThetaDeg = o.value("polarThetaDeg", 0.0f);
            }
            if (!obj.TexPath.empty())
                obj.Tex = Purr::TextureManager::Load(obj.TexPath);
            m_Objects.push_back(obj);
        }

        // regarder la texture (généré procéduralement)
        for (auto& obj : m_Objects)
        {
            if (obj.Type == PrimitiveType::Plane && obj.TexPath.empty())
                obj.Tex = m_CheckerTex;
        }
    }

    void LoadMapPreset(const MapPreset& preset)
    {
        SaveSnapshot();

        m_Objects.clear();
        m_MeshCache.clear();
        m_ObjMultiMeshCache.clear();
        m_AnimatedAssetCache.clear();
        m_ObjTexCache.clear();

        SceneObject map;
        map.Name = std::string("Map - ") + preset.Label;
        map.Type = PrimitiveType::Custom;
        map.MeshPath = preset.MeshPath;
        map.Position = preset.MapPosition;
        map.Scale = preset.MapScale;
        map.Mat.Diffuse = { 1.0f, 1.0f, 1.0f };
        map.Mat.Model = IlluminationModel::Phong;

        std::string texPath;
        Purr::LoadOBJ(preset.MeshPath, texPath);
        if (!texPath.empty()) {
            map.TexPath = texPath;
            map.Tex = Purr::TextureManager::Load(texPath);
        }

        m_Objects.push_back(map);
        m_Selection.clear();
        m_Selected = 0;
        m_Selection.insert(0);

        m_CurrentMapName = preset.Label;
        m_CurrentMapSpawn = preset.PlayerSpawn;
        m_CurrentPlayerScale = preset.PlayerScale;
    }

private:
    struct CachedObjPart {
        std::shared_ptr<Purr::VertexArray> Mesh = nullptr;
        std::string TexPath;
        std::vector<std::string> BoneNames;
        std::string AnimNodeName;
        glm::mat4   AnimNodeBindLocal = glm::mat4(1.0f);
        glm::mat4   AnimNodeBindGlobal = glm::mat4(1.0f);
        glm::vec3 DiffuseTint = glm::vec3(1.0f);
        glm::vec3 BoundsMin = glm::vec3(0.0f);
        glm::vec3 BoundsMax = glm::vec3(0.0f);
    };

    struct StaticColliderAABB {
        glm::vec3 Min = glm::vec3(0.0f);
        glm::vec3 Max = glm::vec3(0.0f);
        bool IsFloor = true;
    };

    struct SavedCameraState {
        Purr::ProjectionMode Mode = Purr::ProjectionMode::Perspective;
        glm::vec3 Target = { 0.0f, 0.0f, 0.0f };
        float Radius = 5.0f;
        float Azimuth = 45.0f;
        float Elevation = 25.0f;
    };

    struct Projectile {
        glm::vec3 Position = { 0,0,0 };
        glm::vec3 Velocity = { 0,0,0 };
        float Life = 0.0f;
        float Radius = 0.08f;
        bool Active = true;
    };

    std::shared_ptr<Purr::VertexArray> GetMeshForObject(SceneObject& obj)
    {
        if (obj.Type == PrimitiveType::Folder)
            return nullptr;
        if (obj.Type == PrimitiveType::Custom) {
            if (!obj.Parts.empty())
                return obj.Parts.front().Mesh;

            auto multiIt = m_ObjMultiMeshCache.find(obj.MeshPath);
            if (multiIt != m_ObjMultiMeshCache.end()) {
                obj.Parts.clear();
                for (const auto& cachedPart : multiIt->second) {
                    SceneObject::RenderPart part;
                    part.Mesh = cachedPart.Mesh;
                    part.TexturePath = cachedPart.TexPath;
                    part.BoneNames = cachedPart.BoneNames;
                    part.AnimNodeName = cachedPart.AnimNodeName;
                    part.AnimNodeBindLocal = cachedPart.AnimNodeBindLocal;
                    part.AnimNodeBindGlobal = cachedPart.AnimNodeBindGlobal;
                    part.DiffuseTint = cachedPart.DiffuseTint;
                    part.BoundsMin = cachedPart.BoundsMin;
                    part.BoundsMax = cachedPart.BoundsMax;
                    if (!part.TexturePath.empty())
                        part.Texture = Purr::TextureManager::Load(part.TexturePath);
                    obj.Parts.push_back(part);
                }

                if (!obj.Tex) {
                    for (auto& part : obj.Parts) {
                        if (part.Texture) {
                            obj.Tex = part.Texture;
                            obj.TexPath = part.TexturePath;
                            break;
                        }
                    }
                }

                if (!obj.Parts.empty())
                    return obj.Parts.front().Mesh;
            }

            auto it = m_MeshCache.find(obj.MeshPath);
            if (it != m_MeshCache.end()) {
                // Cache hit — assigner la texture quand même si manquante
                if (!obj.Tex) {
                    auto texIt = m_ObjTexCache.find(obj.MeshPath);
                    if (texIt != m_ObjTexCache.end() && !texIt->second.empty())
                        obj.Tex = Purr::TextureManager::Load(texIt->second);
                }
                return it->second;
            }

            namespace fs = std::filesystem;
            std::string ext = fs::path(obj.MeshPath).extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });

            // Les avatars peuvent etre en FBX/GLTF: on passe par Assimp (LoadMeshFile).
            if (ext != ".obj") {
                LoadedAnimatedAsset* animAsset = nullptr;
                auto assetIt = m_AnimatedAssetCache.find(obj.MeshPath);
                if (assetIt == m_AnimatedAssetCache.end())
                    assetIt = m_AnimatedAssetCache.emplace(obj.MeshPath, LoadAnimatedAsset(obj.MeshPath)).first;
                animAsset = &assetIt->second;
                auto& meshes = animAsset->Meshes;
                if (!meshes.empty()) {
                    std::vector<CachedObjPart> cachedParts;
                    cachedParts.reserve(meshes.size());
                    obj.Parts.clear();

                    for (const auto& lm : meshes) {
                        auto vaPart = BuildVertexArrayFromLoadedMesh(lm);
                        if (!vaPart)
                            continue;

                        const std::string resolvedTex = ResolveImportedTexturePath(obj.MeshPath, lm.TexturePath);

                        CachedObjPart cachePart;
                        cachePart.Mesh = vaPart;
                        cachePart.TexPath = resolvedTex;
                        for (const auto& b : lm.Bones)
                            cachePart.BoneNames.push_back(b.Name);
                        cachePart.AnimNodeName = lm.SourceNodeName;
                        cachePart.AnimNodeBindLocal = lm.SourceNodeBindLocal;
                        cachePart.AnimNodeBindGlobal = lm.SourceNodeBindGlobal;
                        cachedParts.push_back(cachePart);

                        SceneObject::RenderPart part;
                        part.Mesh = vaPart;
                        part.TexturePath = resolvedTex;
                        part.BoneNames = cachePart.BoneNames;
                        part.AnimNodeName = cachePart.AnimNodeName;
                        part.AnimNodeBindLocal = cachePart.AnimNodeBindLocal;
                        part.AnimNodeBindGlobal = cachePart.AnimNodeBindGlobal;
                        if (!resolvedTex.empty())
                            part.Texture = Purr::TextureManager::Load(resolvedTex);
                        obj.Parts.push_back(part);
                    }

                    if (!cachedParts.empty()) {
                        if (cachedParts.size() == 1 && cachedParts.front().TexPath.empty()) {
                            const std::string fallbackDiffuse = GuessDiffuseTextureNearModel(obj.MeshPath);
                            if (!fallbackDiffuse.empty()) {
                                cachedParts.front().TexPath = fallbackDiffuse;
                                obj.Parts.front().TexturePath = fallbackDiffuse;
                                obj.Parts.front().Texture = Purr::TextureManager::Load(fallbackDiffuse);
                            }
                        }

                        m_ObjMultiMeshCache[obj.MeshPath] = cachedParts;
                        m_MeshCache[obj.MeshPath] = cachedParts.front().Mesh;
                        m_ObjTexCache[obj.MeshPath] = cachedParts.front().TexPath;

                        if (!cachedParts.front().TexPath.empty() && !obj.Tex) {
                            obj.TexPath = cachedParts.front().TexPath;
                            obj.Tex = Purr::TextureManager::Load(cachedParts.front().TexPath);
                        }
                        return cachedParts.front().Mesh;
                    }
                }
            }

            std::vector<Purr::ObjSubmesh> submeshes;
            std::string firstTexPath;
            if (Purr::LoadOBJMulti(obj.MeshPath, submeshes, firstTexPath) && !submeshes.empty()) {
                std::vector<CachedObjPart> cachedParts;
                cachedParts.reserve(submeshes.size());
                obj.Parts.clear();

                for (const auto& sm : submeshes) {
                    if (!sm.Mesh)
                        continue;

                    CachedObjPart cachePart;
                    cachePart.Mesh = sm.Mesh;
                    cachePart.TexPath = sm.TexturePath;
                    cachePart.DiffuseTint = sm.DiffuseTint;
                    cachePart.BoundsMin = sm.BoundsMin;
                    cachePart.BoundsMax = sm.BoundsMax;
                    cachedParts.push_back(cachePart);

                    SceneObject::RenderPart part;
                    part.Mesh = sm.Mesh;
                    part.TexturePath = sm.TexturePath;
                    part.DiffuseTint = sm.DiffuseTint;
                    part.BoundsMin = sm.BoundsMin;
                    part.BoundsMax = sm.BoundsMax;
                    if (!part.TexturePath.empty())
                        part.Texture = Purr::TextureManager::Load(part.TexturePath);
                    obj.Parts.push_back(part);
                }

                if (!cachedParts.empty()) {
                    m_ObjMultiMeshCache[obj.MeshPath] = cachedParts;
                    m_MeshCache[obj.MeshPath] = cachedParts.front().Mesh;
                    m_ObjTexCache[obj.MeshPath] = firstTexPath;

                    if (!firstTexPath.empty() && !obj.Tex) {
                        obj.TexPath = firstTexPath;
                        obj.Tex = Purr::TextureManager::Load(firstTexPath);
                    }
                    return cachedParts.front().Mesh;
                }
            }

            std::string texPath;
            auto va = Purr::LoadOBJ(obj.MeshPath, texPath);
            if (va) {
                m_MeshCache[obj.MeshPath] = va;
                m_ObjTexCache[obj.MeshPath] = texPath;   // ← stocker le texPath aussi
                if (!texPath.empty() && !obj.Tex)
                    obj.Tex = Purr::TextureManager::Load(texPath);
            }
            return va;
        }
        switch (obj.Type) {
        case PrimitiveType::Plane:         return m_PlaneVA;
        case PrimitiveType::Triangle:      return m_TriangleVA;
        case PrimitiveType::Circle:        return m_CircleVA;
        case PrimitiveType::RegularPolygon:return m_RegPolygonVA;
        case PrimitiveType::Ellipse:       return m_EllipseVA;
        case PrimitiveType::Sphere:        return m_SphereVA;
        case PrimitiveType::PolarMarker:  return m_SphereVA;
        default:                           return m_VA; // Cube
        }
    }



    // -----------------------------------------------------------------------
    // Hit-test : dispatche selon le mode courant
    // -----------------------------------------------------------------------
    GizmoAxis HitTestGizmo(glm::vec2 mouseNDC)
    {
        if (m_GizmoMode == GizmoMode::Hand)
            return GizmoAxis::None;
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

        glm::vec3 pos = m_Objects[m_Selected].GetWorldPosition(m_Objects);
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

        glm::vec3 pos = m_Objects[m_Selected].GetWorldPosition(m_Objects);
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

        glm::vec3 pos = m_Objects[m_Selected].GetWorldPosition(m_Objects);
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

    void BuildSphereMesh(int stacks = 16, int sectors = 32) {
        std::vector<float> verts;
        std::vector<uint32_t> indices;
        float r = 0.5f;
        for (int i = 0; i <= stacks; i++) {
            float phi = glm::pi<float>() * i / stacks;
            float y = r * cosf(phi);
            float sinPhi = sinf(phi);
            for (int j = 0; j <= sectors; j++) {
                float theta = 2.0f * glm::pi<float>() * j / sectors;
                float x = r * sinPhi * cosf(theta);
                float z = r * sinPhi * sinf(theta);
                float nx = sinPhi * cosf(theta);
                float ny = cosf(phi);
                float nz = sinPhi * sinf(theta);
                float u = (float)j / sectors;
                float v = (float)i / stacks;
                verts.insert(verts.end(), { x, y, z, nx, ny, nz, u, v });
            }
        }
        for (int i = 0; i < stacks; i++) {
            for (int j = 0; j < sectors; j++) {
                uint32_t a = i * (sectors + 1) + j;
                uint32_t b = a + sectors + 1;
                indices.insert(indices.end(), { a, b, a + 1, b, b + 1, a + 1 });
            }
        }
        m_SphereVA = std::make_shared<Purr::VertexArray>();
        auto vb = std::make_shared<Purr::VertexBuffer>(verts.data(), (uint32_t)(verts.size() * sizeof(float)));
        vb->SetLayout({ {Purr::ShaderDataType::Float3,"a_Position"},{Purr::ShaderDataType::Float3,"a_Normal"},{Purr::ShaderDataType::Float2,"a_TexCoord"} });
        m_SphereVA->AddVertexBuffer(vb);
        m_SphereVA->SetIndexBuffer(std::make_shared<Purr::IndexBuffer>(indices.data(), (uint32_t)indices.size()));
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
            uniform float u_Opacity;
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
                color=vec4(r,u_Opacity);
            })";
        m_Shader = std::make_shared<Purr::Shader>(vs, fs);
    }
    // vec3 tc=texture(u_Texture,v_TexCoord*4.0).rgb*u_MatDiffuse;
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
            uniform float u_TilingFactor;
            uniform int u_UseProceduralTex;
            uniform float u_ProceduralTexScale;
            uniform float u_Opacity;
            out vec4 color;

            void main(){
               vec2 uv = v_TexCoord * u_TilingFactor;
                vec3 baseTex;
                if (u_UseProceduralTex == 1) {
                    vec2 puv = v_TexCoord * u_ProceduralTexScale;
                    float checker = mod(floor(puv.x) + floor(puv.y), 2.0);
                    vec3 cA = vec3(0.93, 0.92, 0.88);
                    vec3 cB = vec3(0.22, 0.22, 0.24);
                    // petit bruit procedural deterministic pour casser l'uniformite.
                    float n = fract(sin(dot(floor(puv), vec2(12.9898,78.233))) * 43758.5453);
                    baseTex = mix(cA, cB, checker) * (0.86 + 0.14 * n);
                }
                else {
                    baseTex = texture(u_Texture, uv).rgb;
                }
                vec3 tc = baseTex * u_MatDiffuse;

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
                color=vec4(r,u_Opacity);
            })";
        m_TexShader = std::make_shared<Purr::Shader>(vs, fs);
    }

    void BuildSkinnedTexturedShader() {
        std::string vs = R"(
            #version 410 core
            layout(location=0) in vec3 a_Position;
            layout(location=1) in vec3 a_Normal;
            layout(location=2) in vec2 a_TexCoord;
            layout(location=3) in vec4 a_BoneIDs;
            layout(location=4) in vec4 a_BoneWeights;
            uniform mat4 u_VP,u_Model,u_NormalMat;
            uniform mat4 u_BoneMatrices[100];
            out vec3 v_FragPos,v_Normal; out vec2 v_TexCoord;
            void main(){
                ivec4 ids = ivec4(a_BoneIDs);
                vec4 w = a_BoneWeights;
                mat4 skin =
                    w.x * u_BoneMatrices[clamp(ids.x, 0, 99)] +
                    w.y * u_BoneMatrices[clamp(ids.y, 0, 99)] +
                    w.z * u_BoneMatrices[clamp(ids.z, 0, 99)] +
                    w.w * u_BoneMatrices[clamp(ids.w, 0, 99)];
                if (dot(w, vec4(1.0)) <= 0.0001)
                    skin = mat4(1.0);
                vec4 localPos = skin * vec4(a_Position,1.0);
                vec3 localNrm = mat3(skin) * a_Normal;
                vec4 wp=u_Model*localPos;
                v_FragPos=vec3(wp); v_Normal=normalize(mat3(u_NormalMat)*localNrm);
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
            uniform float u_TilingFactor;
            uniform int u_UseProceduralTex;
            uniform float u_ProceduralTexScale;
            uniform float u_Opacity;
            out vec4 color;
            void main(){
                vec2 uv = v_TexCoord * u_TilingFactor;
                vec3 baseTex;
                if (u_UseProceduralTex == 1) {
                    vec2 puv = v_TexCoord * u_ProceduralTexScale;
                    float checker = mod(floor(puv.x) + floor(puv.y), 2.0);
                    vec3 cA = vec3(0.93, 0.92, 0.88);
                    vec3 cB = vec3(0.22, 0.22, 0.24);
                    float n = fract(sin(dot(floor(puv), vec2(12.9898,78.233))) * 43758.5453);
                    baseTex = mix(cA, cB, checker) * (0.86 + 0.14 * n);
                } else {
                    baseTex = texture(u_Texture, uv).rgb;
                }
                vec3 tc = baseTex * u_MatDiffuse;
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
                color=vec4(r,u_Opacity);
            })";
        m_TexSkinShader = std::make_shared<Purr::Shader>(vs, fs);
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
    // Rename inline
    int  m_RenamingIndex = -1;
    char m_RenameBuffer[128] = {};
    bool m_RenameFocusPending = false;


    // Scene
    std::vector<SceneObject>     m_Objects;
    int                          m_Selected = 0;          // pivot principal (gizmo)
    bool                         m_ShowHiddenInSceneList = false;
    std::unordered_set<int>      m_Selection;             // tous les sélectionnés
    std::vector<SceneObject>     m_Clipboard;             // copier/coller


    Light m_Lights[4];
    float m_AmbientStrength = 0.15f;

    // Renderer
    std::shared_ptr<Purr::VertexArray>  m_VA, m_PlaneVA, m_BBoxVA;
    std::shared_ptr<Purr::VertexArray>  m_TriangleVA, m_CircleVA, m_RegPolygonVA, m_EllipseVA, m_SphereVA;
    std::shared_ptr<Purr::Shader>       m_Shader, m_TexShader, m_TexSkinShader, m_WireShader;
    std::shared_ptr<Purr::Texture>      m_CheckerTex, m_PlayButtonTex, m_StopButtonTex;
    std::shared_ptr<Purr::Framebuffer>  m_Framebuffer;
    std::shared_ptr<Purr::Framebuffer>  m_ViewTopFramebuffer;
    std::shared_ptr<Purr::Framebuffer>  m_ViewFrontFramebuffer;
    std::shared_ptr<Purr::Framebuffer>  m_ViewRightFramebuffer;
    Purr::Camera                        m_Camera;
    enum class MultiViewLayout { Single = 0, Dual, Quad };
    MultiViewLayout                     m_ViewLayout = MultiViewLayout::Single;
    bool                                m_ViewportFullscreen = false;
    glm::vec2                           m_AuxTopSize = { 0.0f, 0.0f };
    glm::vec2                           m_AuxFrontSize = { 0.0f, 0.0f };
    glm::vec2                           m_AuxRightSize = { 0.0f, 0.0f };

    // Gizmo
    std::shared_ptr<Purr::VertexArray>  m_ArrowVA;
    std::shared_ptr<Purr::VertexArray>  m_RingVA;
    std::shared_ptr<Purr::VertexArray>  m_ScaleHandleVA;
    std::shared_ptr<Purr::Shader>       m_GizmoShader;
    GizmoMode m_GizmoMode = GizmoMode::Hand;
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
    std::unordered_map<std::string, std::vector<CachedObjPart>>         m_ObjMultiMeshCache;
    std::unordered_map<std::string, LoadedAnimatedAsset>                m_AnimatedAssetCache;

    std::unordered_map<std::string, std::string>                        m_ObjTexCache;
    std::string                                                         m_CurrentMapName;
    glm::vec3                                                           m_CurrentMapSpawn = { 0.0f, 0.0f, 0.0f };
    glm::vec3                                                           m_CurrentPlayerScale = { 0.5f, 0.5f, 0.5f };
    PlayerAvatarChoice                                                  m_PlayerAvatarChoice = PlayerAvatarChoice::Male;
    std::string                                                         m_PlayerMaleMeshPath = "assets/models/roblox/male_player/source/12.fbx";
    std::string                                                         m_PlayerFemaleMeshPath = "assets/models/roblox/female_player/baconHair1Tex.obj";
    glm::vec3                                                           m_MaleAvatarScaleMultiplier = { 0.091f, 0.091f, 0.091f };
    glm::vec3                                                           m_MaleAvatarSpawnOffset = { 0.0f, 0.0f, 0.0f };
    glm::vec3                                                           m_FemaleAvatarScaleMultiplier = { 1.0f, 1.0f, 1.0f };
    glm::vec3                                                           m_FemaleAvatarSpawnOffset = { 0.0f, 0.0f, 0.0f };

    // Play mode
    enum class EngineState { Editor, Playing };
    EngineState              m_State = EngineState::Editor;
    std::vector<SceneObject> m_SavedScene;
    std::vector<StaticColliderAABB> m_PlayStaticColliders;
    glm::vec3                     m_PlayerVelocity = { 0.0f, 0.0f, 0.0f };
    bool                          m_PlayerOnGround = false;
    float                         m_Gravity = -18.0f;
    float                         m_JumpVelocity = 6.0f;
    float                         m_ColliderShrinkXZ = 0.12f;
    float                         m_FloorAspectThreshold = 0.20f;
    bool                          m_EnableAutoSpawnSnap = true;
    float                         m_AutoSpawnSearchHeight = 8.0f;
    float                         m_AutoSpawnClearance = 0.02f;
    SavedCameraState              m_SavedCamera;
    float                         m_PlayCamDistance = 6.0f;
    float                         m_PlayCamAzimuth = 35.0f;
    float                         m_PlayCamElevation = 20.0f;
    float                         m_PlayCamTargetHeight = 0.9f;
    float                         m_PlayCamFollowSpeed = 10.0f;
    float                         m_PlayCamMinDist = 0.12f;
    float                         m_PlayCamMaxDist = 28.0f;
    float                         m_PlayCamFPThreshold = 1.05f;
    float                         m_PlayFPYawDeg = 35.0f;
    float                         m_PlayFPPitchDeg = 0.0f;
    float                         m_PlayFPPitchMinDeg = -88.0f;
    float                         m_PlayFPPitchMaxDeg = 88.0f;
    float                         m_PlayFPMouseSens = 0.11f;
    float                         m_PlayFPSphereRadius = 1.0f;
    // Reglages 1ere personne selon avatar
    float                         m_PlayFPEyeHeightBaseFemale = 0.00f;
    float                         m_PlayFPEyeHeightScaleFemale = 0.68f;
    float                         m_PlayFPEyeForwardBaseFemale = 0.00f;
    float                         m_PlayFPEyeForwardScaleFemale = 0.35f;
    float                         m_PlayFPEyeHeightBaseMale = 0.42f;
    float                         m_PlayFPEyeHeightScaleMale = 1.15f;
    float                         m_PlayFPEyeForwardBaseMale = 0.04f;
    float                         m_PlayFPEyeForwardScaleMale = 0.45f;
    bool                          m_PlayCameraPrevFirstPerson = false;
    float                         m_PlayerBridgeTime = 0.0f;
    float                         m_SkinnedAnimTime = 0.0f;
    PlayerAnimState               m_PlayerAnimState = PlayerAnimState::Idle;
    std::vector<KeyframeTRS>      m_PlayerIdleKeys;
    std::vector<KeyframeTRS>      m_PlayerWalkKeys;
    std::vector<KeyframeTRS>      m_PlayerJumpKeys;
    std::vector<KeyframeTRS>      m_PlayerFallKeys;
    std::vector<Projectile>       m_Projectiles;
    float                         m_BazookaCooldown = 0.23f;
    float                         m_BazookaCooldownTimer = 0.0f;
    bool                          m_PlayBazookaEquipped = false;
    bool                          m_PlayEmoteMenuOpen = false;
    int                           m_PlayMaleEmoteSlotIndex = -1;
    bool                          m_MaleEmoteClipLoaded[kMaleEmoteSlotCount]{};
    float                         m_PlayMaleEmoteTime = 0.0f;
    float                         m_PlayEmoteDebugTimer = 0.0f;
    LoadedAnimationClip           m_MaleEmoteClips[kMaleEmoteSlotCount];
    float                         m_ProjectileSpeed = 15.0f;
    float                         m_ProjectileLifetime = 2.0f;
    float                         m_ProjectileGravity = -1.2f;
    float                         m_ExplosionRadius = 1.85f;
    float                         m_ExplosionForce = 6.0f;
    float                         m_BazookaAnimTime = 0.0f;
    int                           m_ImGuiThemeIndex = 0;
};

// -----------------------------------------------------------------------
class Sandbox : public Purr::Application {
public:
    Sandbox() { PushLayer(new ExampleLayer()); }
    ~Sandbox() {}
};

Purr::Application* Purr::CreateApplication() { return new Sandbox(); }