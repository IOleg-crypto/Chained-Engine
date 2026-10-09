#include "engine/graphics/api/graphics_device.h"
#include "opengl/gl_device.h"
#include <vector>
#include <mutex>

namespace Chained
{

	void GraphicsDevice::EnqueueResourceDeletion(std::function<void()> deleter)
	{
		if (!deleter)
		{
			return;
		}
		std::lock_guard<std::mutex> lock(m_DeletionMutex);
		m_DeletionQueue.push_back(std::move(deleter));
	}

	void GraphicsDevice::ProcessResourceDeletions()
	{
		std::vector<std::function<void()>> queueToProcess;
		{
			std::lock_guard<std::mutex> lock(m_DeletionMutex);
			if (m_DeletionQueue.empty())
			{
				return;
			}
			queueToProcess.swap(m_DeletionQueue);
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
