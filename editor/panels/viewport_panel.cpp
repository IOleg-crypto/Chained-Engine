#include "viewport_panel.h"
#include "editor/layer.h"
#include "editor/asset_types.h"
#include "editor/events.h"
#include "engine/app/application.h"
#include "engine/core/events/events.h"
#include "engine/core/input.h"
#include "engine/core/service_locator.h"
#include "engine/graphics/api/framebuffer.h"
#include "engine/graphics/pipeline/scene_renderer.h"
#include "engine/ui/widget_renderer.h"
#include "engine/project/project.h"
#include "engine/scene/scene.h"
#include "engine/scene/scene_events.h"
#include "engine/scene/prefab_serializer.h"
#include "events.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <GLFW/glfw3.h>

namespace Chained
{

	ViewportPanel::ViewportPanel(ImVec2& editorViewportSize)
		: m_EditorViewportSize(editorViewportSize)
	{
		m_Name = "Viewport";

		m_CameraController = std::make_unique<EditorCameraController>();
		m_Toolbar = std::make_unique<ViewportToolbar>(m_Gizmo, *m_CameraController);

		uint32_t w = 1280, h = 720;
		if (Application::Get().GetWindow().GetNativeWindow())
		{
			w = Application::Get().GetWindow().GetWidth() > 0 ? Application::Get().GetWindow().GetWidth() : 1280;
			h = Application::Get().GetWindow().GetHeight() > 0 ? Application::Get().GetWindow().GetHeight() : 720;
		}
		m_Renderer.Init(w, h);
	}

	ViewportPanel::~ViewportPanel()
	{
		if (m_CursorLocked && m_LockedWindow)
		{
			glfwSetInputMode(m_LockedWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			m_CursorLocked = false;
			m_LockedWindow = nullptr;
		}
	}

	Camera3D ViewportPanel::GetActiveOrEditorCamera(Scene* scene) const
	{
		if (!scene)
		{
			return {};
		}
		auto activeCameraOpt = SceneRenderer::GetActiveCamera(scene->GetRegistry());
		if (activeCameraOpt.has_value() && EditorLayer::Get().GetSceneManager().GetSceneState() == SceneState::Play)
		{
			return activeCameraOpt.value();
		}
		return m_CameraController->ToCamera3D();
	}

	void ViewportPanel::HandleResize(const ImVec2& viewportSize, Scene* activeScene)
	{
		if (viewportSize.x != m_ViewportSize.x || viewportSize.y != m_ViewportSize.y)
		{
			m_ViewportSize = {viewportSize.x, viewportSize.y};
			if (m_ViewportSize.x > 0 && m_ViewportSize.y > 0)
			{
				m_Renderer.Resize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
				m_EditorViewportSize = {m_ViewportSize.x, m_ViewportSize.y};
				m_CameraController->SetViewportSize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);

				if (activeScene)
				{
					activeScene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
				}
			}
		}

		m_Renderer.CheckMSAAChange();

		if (m_ViewportSize.x > 0 && m_ViewportSize.y > 0)
		{
			m_Renderer.Resize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
		}
	}

	void ViewportPanel::HandleDragDrop(Scene* activeScene)
	{
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
			{
				const char* path = (const char*)payload->Data;
				std::filesystem::path filepath = std::filesystem::path(path);
				std::string ext = filepath.extension().string();
				std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

				if (ext == ".chscene")
				{
					EditorLayer::Get().GetSceneManager().OpenScene(filepath);
				}
				else if (ext == ".chprefab")
				{
					PrefabSerializer::Deserialize(activeScene, filepath.string());
				}
				else if (ext == ".gltf" || ext == ".glb" || ext == ".obj")
				{
					std::string filename = filepath.stem().string();
					Entity entity = activeScene->CreateEntity(filename);
					auto& modelcomp = entity.AddComponent<ModelComponent>();
					modelcomp.ModelPath = Project::GetActive()->GetRelativePath(filepath);
					SelectEntity(entity, activeScene);
				}
			}
			ImGui::EndDragDropTarget();
		}
	}

	void ViewportPanel::RenderOverlays(Scene* activeScene, const ImVec2& viewportSize, const ImVec2& viewportScreenPos)
	{
		auto selectedEntity = EditorLayer::Get().GetEditorState().SelectedEntity;
		if (selectedEntity)
		{
			if (!activeScene || selectedEntity.GetRegistryPtr() != &activeScene->GetRegistry() ||
				!selectedEntity.IsValid())
			{
				EditorLayer::Get().GetEditorState().SelectedEntity = {};
				selectedEntity = {};
			}
		}
		bool isUISelected = selectedEntity && selectedEntity.HasComponent<ControlComponent>();
		auto camera = GetActiveOrEditorCamera(activeScene);

		ImGui::SetCursorScreenPos(viewportScreenPos);

		// Gizmo handling
		m_Gizmo.RenderAndHandle(!isUISelected ? m_Gizmo.GetCurrentTool() : GizmoType::NONE, viewportScreenPos,
								viewportSize, camera);

		// Game UI Overlay
		ImVec2 canvasOrigin = viewportScreenPos;
		if (auto* widgetRenderer = ServiceLocator::TryGet<WidgetRenderer>())
		{
			widgetRenderer->DrawCanvas(activeScene, canvasOrigin, viewportSize,
									   EditorLayer::Get().GetSceneManager().GetSceneState() == SceneState::Edit);
		}

		// Script UI Overlay (OnGUI)
		SceneState sceneState = EditorLayer::Get().GetSceneManager().GetSceneState();
		if (activeScene && (sceneState == SceneState::Play || sceneState == SceneState::Simulate))
		{
			ImGui::SetCursorScreenPos(ImVec2(viewportScreenPos.x + 10.0f, viewportScreenPos.y + 10.0f));
			activeScene->OnRenderUI();
		}

		// Selection Highlight for UI entities
		if (isUISelected && selectedEntity && EditorLayer::Get().GetSceneManager().GetSceneState() == SceneState::Edit)
		{
			auto* widgetRenderer = ServiceLocator::TryGet<WidgetRenderer>();
			auto rect = widgetRenderer ? widgetRenderer->GetEntityRect(selectedEntity) : UIRect{0, 0, 0, 0};

			ImVec2 p1 = ImVec2(rect.x, rect.y);
			ImVec2 p2 = ImVec2(p1.x + rect.width, p1.y + rect.height);

			ImGui::GetWindowDrawList()->AddRect(p1, p2, IM_COL32(255, 255, 0, 255), 0, 0, 2.0f);

			m_UIManipulator.OnImGuiRender(selectedEntity, viewportScreenPos, viewportSize);

			if (ImGui::IsMouseHoveringRect(p1, p2))
			{
				ImGui::GetWindowDrawList()->AddRect(p1, p2, IM_COL32(0, 255, 0, 255), 0, 0, 1.0f);
			}
		}
	}

	void ViewportPanel::OnImGuiRender(bool readOnly)
	{
		if (!m_IsOpen)
		{
			return;
		}

		ImGuiViewport* vp = ImGui::GetWindowViewport();
		if (vp && vp->PlatformHandle)
		{
			m_PlatformWindow = static_cast<GLFWwindow*>(vp->PlatformHandle);
		}
		else
		{
			m_PlatformWindow = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		}

		auto activeScene = EditorLayer::Get().GetSceneManager().GetActiveScene();

		std::string sceneName = "None";
		if (activeScene && !activeScene->GetSettings().Name.empty())
		{
			sceneName = activeScene->GetSettings().Name;
		}
		std::string title = m_Name + " [" + sceneName + "]###" + m_Name;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0, 0});
		ImGui::Begin(title.c_str(), &m_IsOpen);

		ImVec2 viewportSize = ImGui::GetContentRegionAvail();
		ImVec2 viewportScreenPos = ImGui::GetCursorScreenPos();

		HandleResize(viewportSize, activeScene.get());

		if (!activeScene || viewportSize.x <= 0 || viewportSize.y <= 0)
		{
			ImGui::End();
			ImGui::PopStyleVar();
			return;
		}

		m_Focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
		m_Hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);

		// Rendering
		if (!activeScene->IsStartingUp())
		{
			auto camera = GetActiveOrEditorCamera(activeScene.get());
			m_Renderer.RenderScene(activeScene.get(), camera);
		}

		// UI Image
		if (!m_Renderer.IsValid())
		{
			ImGui::End();
			ImGui::PopStyleVar();
			return;
		}
		uint32_t finalTextureID = m_Renderer.GetViewportFramebuffer()->GetColorAttachmentRendererID();
		viewportScreenPos = ImGui::GetCursorScreenPos();

		ImGui::Image((ImTextureID)(uintptr_t)finalTextureID, viewportSize, {0, 1}, {1, 0});

		bool isTransitioning = EditorLayer::Get().GetSceneManager().IsTransitioning();
		if (activeScene->IsStartingUp() || isTransitioning)
		{
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			ImVec2 p0 = ImGui::GetItemRectMin();
			ImVec2 p1 = ImGui::GetItemRectMax();

			drawList->AddRectFilled(p0, p1, IM_COL32(15, 15, 20, 200));

			std::string status = EditorLayer::Get().GetSceneManager().GetLoadingStatus();
			if (status.empty())
			{
				status = "Loading Scene...";
			}
			const char* text = status.c_str();
			ImVec2 textSize = ImGui::CalcTextSize(text);
			ImVec2 textPos = ImVec2(p0.x + (p1.x - p0.x - textSize.x) * 0.5f, p0.y + (p1.y - p0.y - textSize.y) * 0.5f);
			drawList->AddText(textPos, IM_COL32(255, 255, 255, 255), text);
		}

		HandleDragDrop(activeScene.get());
		RenderOverlays(activeScene.get(), viewportSize, viewportScreenPos);

		auto camera = GetActiveOrEditorCamera(activeScene.get());
		m_Picking.HandlePicking(activeScene.get(), viewportSize, viewportScreenPos, m_Gizmo, m_UIManipulator, camera);

		m_Toolbar->Render(activeScene.get(), viewportScreenPos);

		if (m_Focused || m_Hovered)
		{
			m_Toolbar->HandleKeyboardShortcuts();
		}

		ImGui::End();
		ImGui::PopStyleVar();
	}

	void ViewportPanel::OnUpdate(Timestep ts)
	{
		bool hasImGui = ImGui::GetCurrentContext() != nullptr;
		bool rightDown = hasImGui ? ImGui::IsMouseDown(ImGuiMouseButton_Right)
								  : Chained::Core::Input::IsMouseButtonDown(Chained::MouseCode::ButtonRight);

#if !CH_PLATFORM_LINUX
		// Unlock cursor if right mouse is released while locked
		if (m_CursorLocked && !rightDown)
		{
			GLFWwindow* win = m_LockedWindow ? m_LockedWindow : m_PlatformWindow;
			if (!win)
			{
				win = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
			}
			if (win)
			{
				glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			}
			m_CursorLocked = false;
			m_LockedWindow = nullptr;
		}
#endif

		// Auto-switch camera 2D mode based on scene type and background mode
		auto activeScene = EditorLayer::Get().GetSceneManager().GetActiveScene();
		if (activeScene)
		{
			SceneType sceneType = activeScene->GetSettings().Type;
			BackgroundMode bgMode = activeScene->GetSettings().Mode;
			auto currentState = std::make_pair(sceneType, bgMode);
			if (currentState != m_LastSceneState)
			{
				bool want2D = (sceneType == SceneType::UI) && (bgMode != BackgroundMode::Environment3D);
				if (m_CameraController->Is2DMode() != want2D)
				{
					m_CameraController->Set2DMode(want2D);
					m_Gizmo.Set2DMode(want2D);
				}
				m_LastSceneState = currentState;
			}
		}

#if !CH_PLATFORM_LINUX
		// Cursor lock for camera rotation (disabled on Linux/WSLg: XWarpPointer under XWayland
		// generates phantom motion events, resulting in violent camera jitter and spinning).
		if ((m_Hovered || m_CursorLocked) && rightDown && !m_CursorLocked)
		{
			GLFWwindow* win = m_PlatformWindow
								  ? m_PlatformWindow
								  : static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
			if (win)
			{
				glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
				m_CursorLocked = true;
				m_LockedWindow = win;
			}
		}
#endif

		// Update editor camera
		SceneState state = EditorLayer::Get().GetSceneManager().GetSceneState();
		if (state == SceneState::Edit || state == SceneState::Simulate)
		{
			auto activeScene = EditorLayer::Get().GetSceneManager().GetActiveScene();
			bool mouseInViewport = m_Hovered || rightDown;
			if (activeScene && mouseInViewport)
			{
				const auto& editorCfg = EditorLayer::Get().GetConfig();
				m_CameraController->SetMoveSpeed(editorCfg.CameraMoveSpeed);
				m_CameraController->SetBoostMultiplier(editorCfg.CameraBoostMultiplier);
				m_CameraController->SetDisableZoom(editorCfg.DisableCameraZoom);
				m_CameraController->SetRotationSpeed(editorCfg.CameraRotationSpeed);
				m_CameraController->SetZoomSpeedMultiplier(editorCfg.CameraZoomSpeedMultiplier);
				m_CameraController->SetFovDegrees(editorCfg.CameraFovDegrees);
				m_CameraController->SetNearClip(editorCfg.CameraNearClip);
				m_CameraController->SetFarClip(editorCfg.CameraFarClip);

				Entity primaryCamera =
					SceneRenderer::GetPrimaryCameraEntity(activeScene->GetRegistry(), activeScene->GetRegistryPtr());
				m_CameraController->OnUpdate(primaryCamera, ts, m_ViewportSize);
			}
		}
	}

	void ViewportPanel::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<ViewportFocusEntityEvent>([this](ViewportFocusEntityEvent& ev) {
			Entity entity = ev.GetEntity();
			if (entity && entity.HasComponent<TransformComponent>())
			{
				auto& transform = entity.GetComponent<TransformComponent>();
				m_CameraController->SetFocalPoint(*reinterpret_cast<const glm::vec3*>(&transform.Translation));
				return true;
			}
			return false;
		});
	}

} // namespace Chained
