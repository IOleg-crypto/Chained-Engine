#ifndef CH_GRAPHICS_DEVICE_H
#define CH_GRAPHICS_DEVICE_H

#include "engine/common/color.h"
#include "engine/common/engine_assert.h"
#include <functional>
#include <memory>

namespace Chained
{
	class VertexArray;

	class GraphicsDevice
	{
	public:
		enum class DepthFunc
		{
			Never = 0,
			Less,
			Equal,
			LEqual,
			Greater,
			NotEqual,
			GEqual,
			Always
		};

		enum class CullMode
		{
			None = 0,
			Front,
			Back,
			FrontAndBack
		};

		enum class BlendFactor
		{
			Zero = 0,
			One,
			SrcColor,
			OneMinusSrcColor,
			DstColor,
			OneMinusDstColor,
			SrcAlpha,
			OneMinusSrcAlpha,
			DstAlpha,
			OneMinusDstAlpha
		};

		enum class PolygonMode
		{
			Fill = 0,
			Line,
			Point
		};

		enum class API
		{
			None = 0,
			OpenGL = 1,
			Vulkan = 2
		};

	public:
		virtual ~GraphicsDevice() = default;

		virtual void Init() = 0;
		virtual void Shutdown()
		{
		}
		virtual void SetViewport(int x, int y, int width, int height) = 0;
		virtual void GetViewport(int* x, int* y, int* width, int* height) const = 0;
		virtual void SetClearColor(const Color& color) = 0;
		virtual void Clear() = 0;
		virtual void Clear(const Color& color)
		{
			SetClearColor(color);
			Clear();
		}

		virtual void SetDepthFunc(DepthFunc func) = 0;
		virtual void SetDepthTest(bool enabled) = 0;
		virtual void SetDepthMask(bool enabled) = 0;
		virtual void EnableDepthTest()
		{
			SetDepthTest(true);
		}
		virtual void DisableDepthTest()
		{
			SetDepthTest(false);
		}
		virtual void EnableDepthMask()
		{
			SetDepthMask(true);
		}
		virtual void DisableDepthMask()
		{
			SetDepthMask(false);
		}

		virtual void SetCullMode(CullMode mode) = 0;
		virtual void SetBlendFunc(BlendFactor src, BlendFactor dst) = 0;
		virtual void SetBlendEnabled(bool enabled) = 0;

		virtual bool IsDepthTestEnabled() const = 0;
		virtual bool IsDepthMaskEnabled() const = 0;
		virtual bool IsBlendEnabled() const = 0;
		virtual bool IsCullFaceEnabled() const = 0;
		virtual PolygonMode GetPolygonMode() const = 0;

		virtual void SetPolygonMode(PolygonMode mode) = 0;
		virtual void SetPolygonOffset(bool enabled, float factor = 0.0f, float units = 0.0f) = 0;

		virtual void SetLineWidth(float width) = 0;

		// Framebuffer operations
		virtual uint32_t GetFramebufferBinding() const = 0;
		virtual void BindFramebuffer(uint32_t fbo) = 0;
		virtual void ClearDepth() = 0;

		virtual void DrawIndexed(const std::shared_ptr<VertexArray>& vertexArray, uint32_t indexCount = 0) = 0;
		virtual void DrawIndexedInstanced(const std::shared_ptr<VertexArray>& vertexArray, uint32_t instanceCount,
										  uint32_t indexCount = 0) = 0;
		virtual void DrawIndexedLines(const std::shared_ptr<VertexArray>& vertexArray, uint32_t indexCount = 0) = 0;
		virtual void DrawLines(const std::shared_ptr<VertexArray>& vertexArray, uint32_t vertexCount) = 0;
		virtual void DrawArrays(uint32_t vertexCount) = 0;
		virtual void DrawArraysInstanced(uint32_t vertexCount, uint32_t instanceCount) = 0;

		virtual void SetTexture(uint32_t slot, uint32_t textureId, bool isCubemap = false) = 0;

		/// Enqueue a GPU resource deletion command safely from any thread.
		/// The deletion will be executed on the main render thread where the GPU context is active.
		static void EnqueueResourceDeletion(std::function<void()> deleter);

		/// Process all pending GPU resource deletions (called on the main render thread).
		static void ProcessResourceDeletions();

		static GraphicsDevice& Get()
		{
			CH_ASSERT(s_Instance, "GraphicsDevice::Get() called before Set() or after Shutdown()!");
			return *s_Instance;
		}
		static void Set(std::unique_ptr<GraphicsDevice> device)
		{
			s_Instance = device.release();
		}
		static API GetAPI()
		{
			return s_API;
		}
		static void SetAPI(API api)
		{
			s_API = api;
		}
		static std::unique_ptr<GraphicsDevice> Create();

	private:
		static GraphicsDevice* s_Instance;
		static API s_API;
	};

	/// RAII guard that saves and restores GPU pipeline state (depth, blend, cull, polygon mode).
	/// Construct before a render pass, destruct after to restore previous state.
	class PipelineStateGuard
	{
	public:
		PipelineStateGuard()
			: m_DepthTest(GraphicsDevice::Get().IsDepthTestEnabled()),
			  m_DepthMask(GraphicsDevice::Get().IsDepthMaskEnabled()),
			  m_Blend(GraphicsDevice::Get().IsBlendEnabled()),
			  m_Cull(GraphicsDevice::Get().IsCullFaceEnabled()),
			  m_PolyMode(GraphicsDevice::Get().GetPolygonMode())
		{
		}

		static PipelineStateGuard Capture()
		{
			return PipelineStateGuard();
		}

		~PipelineStateGuard()
		{
			GraphicsDevice::Get().SetDepthTest(m_DepthTest);
			GraphicsDevice::Get().SetDepthMask(m_DepthMask);
			GraphicsDevice::Get().SetBlendEnabled(m_Blend);
			GraphicsDevice::Get().SetCullMode(m_Cull ? GraphicsDevice::CullMode::Back : GraphicsDevice::CullMode::None);
			GraphicsDevice::Get().SetPolygonMode(m_PolyMode);
		}

		PipelineStateGuard(const PipelineStateGuard&) = delete;
		PipelineStateGuard& operator=(const PipelineStateGuard&) = delete;

		// Fluent builder methods for configuring state changes
		PipelineStateGuard& WithDepthTest()
		{
			GraphicsDevice::Get().EnableDepthTest();
			return *this;
		}
		PipelineStateGuard& WithoutDepthTest()
		{
			GraphicsDevice::Get().DisableDepthTest();
			GraphicsDevice::Get().DisableDepthMask();
			return *this;
		}
		PipelineStateGuard& WithBlend()
		{
			GraphicsDevice::Get().SetBlendEnabled(true);
			GraphicsDevice::Get().SetBlendFunc(GraphicsDevice::BlendFactor::SrcAlpha,
											   GraphicsDevice::BlendFactor::OneMinusSrcAlpha);
			return *this;
		}
		PipelineStateGuard& WithCullNone()
		{
			GraphicsDevice::Get().SetCullMode(GraphicsDevice::CullMode::None);
			return *this;
		}
		PipelineStateGuard& WithWireframeMode()
		{
			GraphicsDevice::Get().SetPolygonMode(GraphicsDevice::PolygonMode::Line);
			return *this;
		}

	private:
		bool m_DepthTest;
		bool m_DepthMask;
		bool m_Blend;
		bool m_Cull;
		GraphicsDevice::PolygonMode m_PolyMode;
	};

} // namespace Chained

#endif // CH_GRAPHICS_DEVICE_H
