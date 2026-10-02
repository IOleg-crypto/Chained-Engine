#include "engine/graphics/api/graphics_device.h"
#include "opengl/gl_device.h"
#include <vector>
#include <mutex>

namespace Chained
{

	GraphicsDevice* GraphicsDevice::s_Instance = nullptr;
	GraphicsDevice::API GraphicsDevice::s_API = GraphicsDevice::API::OpenGL;

	static std::mutex s_DeletionMutex;
	static std::vector<std::function<void()>> s_DeletionQueue;

	void GraphicsDevice::EnqueueResourceDeletion(std::function<void()> deleter)
	{
		if (!deleter)
		{
			return;
		}
		std::lock_guard<std::mutex> lock(s_DeletionMutex);
		s_DeletionQueue.push_back(std::move(deleter));
	}

	void GraphicsDevice::ProcessResourceDeletions()
	{
		std::vector<std::function<void()>> queueToProcess;
		{
			std::lock_guard<std::mutex> lock(s_DeletionMutex);
			if (s_DeletionQueue.empty())
			{
				return;
			}
			queueToProcess.swap(s_DeletionQueue);
		}

		for (auto& deleter : queueToProcess)
		{
			if (deleter)
			{
				deleter();
			}
		}
	}

	std::unique_ptr<GraphicsDevice> GraphicsDevice::Create()
	{
		switch (s_API)
		{
		case GraphicsDevice::API::None:
			return nullptr;
		case GraphicsDevice::API::OpenGL:
			return std::make_unique<GLDevice>();
		default:
			return nullptr;
		}
	}

} // namespace Chained
