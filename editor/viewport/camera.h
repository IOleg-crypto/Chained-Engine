#ifndef CH_EDITOR_CAMERA_H
#define CH_EDITOR_CAMERA_H

#include "engine/common/timestep.h"
#include "engine/scene/camera.h"
#include "engine/scene/entity.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Chained
{

	class EditorCameraController : public Camera
	{
	public:
		EditorCameraController();
		~EditorCameraController() = default;

		void OnUpdate(Entity cameraEntity, Timestep ts, const glm::vec2& viewportSize);

		// Native Projection is provided by Camera parent class.

		const glm::mat4& GetViewMatrix() const
		{
			return m_ViewMatrix;
		}
		glm::mat4 GetViewProjection() const
		{
			return GetProjection() * m_ViewMatrix;
		}

		void SetViewportSize(uint32_t width, uint32_t height)
		{
			m_ViewportWidth = width;
			m_ViewportHeight = height;
			Camera::SetViewportSize(width, height);
			UpdateView();
		}

		glm::vec3 GetUpDirection() const;
		glm::vec3 GetRightDirection() const;
		glm::vec3 GetForwardDirection() const;
		glm::vec3 CalculatePosition() const;
		glm::quat GetOrientation() const;

		float GetPitch() const
		{
			return m_Pitch;
		}
		float GetYaw() const
		{
			return m_Yaw;
		}
		void SetPitch(float pitch)
		{
			m_Pitch = pitch;
			UpdateView();
		}
		void SetYaw(float yaw)
		{
			m_Yaw = yaw;
			UpdateView();
		}

		glm::vec3 GetFocalPoint() const
		{
			return m_FocalPoint;
		}
		void SetFocalPoint(const glm::vec3& focalPoint)
		{
			m_FocalPoint = focalPoint;
			UpdateView();
		}

		float GetDistance() const
		{
			return m_Distance;
		}
		void SetDistance(float distance)
		{
			m_Distance = distance;
			UpdateView();
		}

		void SetMoveSpeed(float speed)
		{
			m_MoveSpeed = speed;
		}
		void SetBoostMultiplier(float multiplier)
		{
			m_BoostMultiplier = multiplier;
		}
		void SetDisableZoom(bool disable)
		{
			m_DisableZoom = disable;
		}
		void SetRotationSpeed(float speed)
		{
			m_RotationSpeed = speed;
		}
		void SetZoomSpeedMultiplier(float multiplier)
		{
			m_ZoomSpeedMultiplier = multiplier;
		}
		void SetFovDegrees(float fov)
		{
			m_FovDegrees = fov;
		}
		void SetNearClip(float near)
		{
			m_NearClip = near;
		}
		void SetFarClip(float far)
		{
			m_FarClip = far;
		}

		float GetBoostMultiplier() const
		{
			return m_BoostMultiplier;
		}
		float GetMoveSpeed() const
		{
			return m_MoveSpeed;
		}
		bool GetDisableZoom() const
		{
			return m_DisableZoom;
		}
		float GetRotationSpeed() const
		{
			return m_RotationSpeed;
		}
		float GetZoomSpeedMultiplier() const
		{
			return m_ZoomSpeedMultiplier;
		}
		float GetFovDegrees() const
		{
			return m_FovDegrees;
		}
		float GetNearClip() const
		{
			return m_NearClip;
		}
		float GetFarClip() const
		{
			return m_FarClip;
		}

		Camera3D ToCamera3D() const;

		void Set2DMode(bool enabled);
		bool Is2DMode() const
		{
			return m_Is2DMode;
		}

		void MousePan(const glm::vec2& delta);
		void MouseRotate(const glm::vec2& delta);
		void MouseZoom(float delta);

	private:
		void UpdateView();

		std::pair<float, float> PanSpeed() const;
		float RotationSpeed() const;
		float ZoomSpeed() const;

	private:
		glm::mat4 m_ViewMatrix = glm::mat4(1.0f);
		glm::vec3 m_FocalPoint = {0.0f, 0.0f, 0.0f};
		float m_Distance = 10.0f;
		float m_Pitch = 0.0f, m_Yaw = 0.0f;

		uint32_t m_ViewportWidth = 1280, m_ViewportHeight = 720;
		float m_MoveSpeed = 10.0f;
		float m_BoostMultiplier = 5.0f;
		bool m_DisableZoom = false;
		float m_RotationSpeed = 1.0f;
		float m_ZoomSpeedMultiplier = 1.0f;
		float m_FovDegrees = 45.0f;
		float m_NearClip = 0.1f;
		float m_FarClip = 10000.0f;

		bool m_Is2DMode = false;
		float m_SavedPitch = 0.0f;
		float m_SavedYaw = 0.0f;
		ProjectionType m_SavedProjectionType = ProjectionType::Perspective;

#if CH_PLATFORM_LINUX
		// Previous-frame button state used to detect the first pressed frame and
		// discard the spurious large delta that WSLg/XWayland reports on button down.
		bool m_WasRightDown = false;
		bool m_WasMiddleDown = false;
		bool m_WasLeftDown = false;
#endif
	};

} // namespace Chained

#endif // CH_EDITOR_CAMERA_H
