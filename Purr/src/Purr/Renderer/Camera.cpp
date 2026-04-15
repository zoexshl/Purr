#include "purrpch.h"
#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Purr {

    Camera::Camera(float fov, float aspectRatio, float nearClip, float farClip)
        : m_FOV(fov), m_AspectRatio(aspectRatio), m_Near(nearClip), m_Far(farClip)
    {
        RecalculateProjection();
        RecalculateView();
    }

    void Camera::SetProjectionMode(ProjectionMode mode)
    {
        m_Mode = mode;
        RecalculateProjection();
    }

    void Camera::SetAspectRatio(float ratio)
    {
        m_AspectRatio = ratio;
        RecalculateProjection();
    }

    void Camera::Orbit(float deltaAzimuth, float deltaElevation)
    {
        m_Azimuth += deltaAzimuth;
        m_Elevation = glm::clamp(m_Elevation + deltaElevation, -89.0f, 89.0f);
        RecalculateView();
    }

    void Camera::Zoom(float delta)
    {
        m_Radius = glm::max(m_Radius - delta, 0.5f);
        RecalculateView();
        // ortho size scales with radius
        if (m_Mode == ProjectionMode::Orthographic)
            RecalculateProjection();
    }

    void Camera::SetTarget(const glm::vec3& target)
    {
        m_Target = target;
        RecalculateView();
    }

    void Camera::SetRadius(float radius)
    {
        m_Radius = glm::max(radius, 0.5f);
        RecalculateView();
        if (m_Mode == ProjectionMode::Orthographic)
            RecalculateProjection();
    }

    void Camera::SetOrbitAngles(float azimuth, float elevation)
    {
        m_Azimuth = azimuth;
        m_Elevation = glm::clamp(elevation, -89.0f, 89.0f);
        RecalculateView();
    }

    void Camera::RecalculateView()
    {
        float az = glm::radians(m_Azimuth);
        float el = glm::radians(m_Elevation);

        glm::vec3 pos = m_Target + glm::vec3(
            m_Radius * cos(el) * cos(az),
            m_Radius * sin(el),
            m_Radius * cos(el) * sin(az)
        );

        m_View = glm::lookAt(pos, m_Target, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    void Camera::RecalculateProjection()
    {
        if (m_Mode == ProjectionMode::Perspective)
        {
            m_Projection = glm::perspective(glm::radians(m_FOV), m_AspectRatio, m_Near, m_Far);
        }
        else
        {
            // ortho size proportional au radius pour que le zoom marche
            float h = m_Radius * glm::tan(glm::radians(m_FOV * 0.5f));
            float w = h * m_AspectRatio;
            m_Projection = glm::ortho(-w, w, -h, h, m_Near, m_Far);
        }
    }

}