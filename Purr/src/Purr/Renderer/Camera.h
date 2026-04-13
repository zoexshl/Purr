#pragma once
#include <glm/glm.hpp>

namespace Purr {

    enum class ProjectionMode { Perspective, Orthographic };

    class Camera {
    public:
        Camera(float fov, float aspectRatio, float nearClip = 0.1f, float farClip = 100.0f);

        void SetProjectionMode(ProjectionMode mode);
        ProjectionMode GetProjectionMode() const { return m_Mode; }

        void SetAspectRatio(float ratio);
        void Orbit(float deltaAzimuth, float deltaElevation);
        void Zoom(float delta);

        const glm::mat4& GetViewMatrix()       const { return m_View; }
        const glm::mat4& GetProjectionMatrix() const { return m_Projection; }
        glm::mat4         GetViewProjection()  const { return m_Projection * m_View; }

        float GetRadius() const { return m_Radius; }

    private:
        void RecalculateView();
        void RecalculateProjection();

        ProjectionMode m_Mode = ProjectionMode::Perspective;

        float m_FOV, m_AspectRatio, m_Near, m_Far;
        float m_Azimuth = 45.0f;
        float m_Elevation = 25.0f;
        float m_Radius = 5.0f;

        glm::vec3 m_Target = { 0.0f, 0.0f, 0.0f };

        glm::mat4 m_View = glm::mat4(1.0f);
        glm::mat4 m_Projection = glm::mat4(1.0f);
    };

}