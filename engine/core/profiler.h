#ifndef CH_PROFILER_H
#define CH_PROFILER_H

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "engine/common/base.h"
#include "engine/core/service.h"
#include "engine/core/service_locator.h"

namespace Chained
{
	struct ProfileResult
	{
		std::string Name;
		long long Start;
		long long End;
		float DurationMS;
		uint32_t ThreadID;
	};

	struct InstrumentationSession
	{
		std::string Name;
	};

	struct ProfilerStats
	{
		// Rendering
		uint32_t DrawCalls = 0;
		uint32_t MeshCount = 0;
		uint32_t TextureCount = 0;

		// Scene
		uint32_t EntityCount = 0;
		uint32_t ColliderCount = 0;
	};

	class Instrumentor : public Service
	{
	public:
		Instrumentor()
			: m_ProfileCount(0)
		{
		}

		void Initialize() override
		{
		}
		void Shutdown() override
		{
			EndSession();
		}

		void BeginSession(const std::string& name, const std::string& filepath = "results.json")
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			m_OutputStream.open(filepath);
			WriteHeader();
			m_CurrentSession = std::make_unique<InstrumentationSession>(InstrumentationSession{name});
		}

		void EndSession()
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			WriteFooter();
			m_OutputStream.close();
			m_CurrentSession.reset();
			m_ProfileCount = 0;
		}

		void WriteProfile(const ProfileResult& result)
		{
			std::lock_guard<std::mutex> lock(m_Mutex);

			m_FrameResults.push_back(result);

			if (m_OutputStream.is_open())
			{
				if (m_ProfileCount++ > 0)
				{
					m_OutputStream << ",";
				}

				std::string name = result.Name;
				std::replace(name.begin(), name.end(), '"', '\'');

				m_OutputStream << "{";
				m_OutputStream << "\"cat\":\"function\",";
				m_OutputStream << "\"dur\":" << (result.End - result.Start) << ',';
				m_OutputStream << "\"name\":\"" << name << "\",";
				m_OutputStream << "\"ph\":\"X\",";
				m_OutputStream << "\"pid\":1,";
				m_OutputStream << "\"tid\":" << result.ThreadID << ",";
				m_OutputStream << "\"ts\":" << result.Start;
				m_OutputStream << "}";

				m_OutputStream.flush();
			}
		}

		void ClearFrameResults()
		{
			std::lock_guard<std::mutex> lock(m_Mutex);
			m_FrameResults.clear();
		}

		const std::vector<ProfileResult>& GetFrameResults() const
		{
			return m_FrameResults;
		}

		// --- Frame-level stats (formerly Profiler) ---

		void BeginFrame()
		{
			s_LastFrameResults = m_FrameResults;
			ClearFrameResults();
			s_Stats.DrawCalls = 0;
			s_Stats.MeshCount = 0;
			s_Stats.TextureCount = 0;
		}

		void UpdateStats(const ProfilerStats& stats)
		{
			s_Stats.DrawCalls += stats.DrawCalls;
			s_Stats.MeshCount += stats.MeshCount;
			s_Stats.TextureCount += stats.TextureCount;
			if (stats.EntityCount > 0)
			{
				s_Stats.EntityCount = stats.EntityCount;
			}
			if (stats.ColliderCount > 0)
			{
				s_Stats.ColliderCount = stats.ColliderCount;
			}
		}

		const ProfilerStats& GetStats() const
		{
			return s_Stats;
		}
		const std::vector<ProfileResult>& GetLastFrameResults() const
		{
			return s_LastFrameResults;
		}

		void WriteHeader()
		{
			m_OutputStream << "{\"otherData\": {},\"traceEvents\":[";
			m_OutputStream.flush();
		}

		void WriteFooter()
		{
			m_OutputStream << "]}";
			m_OutputStream.flush();
		}

		static Instrumentor* TryGet()
		{
			return ServiceLocator::TryGet<Instrumentor>();
		}

	private:
		std::unique_ptr<InstrumentationSession> m_CurrentSession;
		std::ofstream m_OutputStream;
		int m_ProfileCount;
		std::vector<ProfileResult> m_FrameResults;
		std::mutex m_Mutex;

		ProfilerStats s_Stats{};
		std::vector<ProfileResult> s_LastFrameResults;
	};

	class InstrumentationTimer
	{
	public:
		InstrumentationTimer(const char* name)
			: m_Name(name),
			  m_Stopped(false)
		{
			m_StartTimepoint = std::chrono::high_resolution_clock::now();
		}

		~InstrumentationTimer()
		{
			if (!m_Stopped)
			{
				Stop();
			}
		}

		void Stop()
		{
			auto endTimepoint = std::chrono::high_resolution_clock::now();

			long long start =
				std::chrono::time_point_cast<std::chrono::microseconds>(m_StartTimepoint).time_since_epoch().count();
			long long end =
				std::chrono::time_point_cast<std::chrono::microseconds>(endTimepoint).time_since_epoch().count();

			float durationMS = (float)(end - start) / 1000.0f;

			uint32_t threadID = (uint32_t)std::hash<std::thread::id>{}(std::this_thread::get_id());
			if (auto* inst = Instrumentor::TryGet())
			{
				inst->WriteProfile({m_Name, start, end, durationMS, threadID});
			}

			m_Stopped = true;
		}

	private:
		const char* m_Name;
		std::chrono::time_point<std::chrono::high_resolution_clock> m_StartTimepoint;
		bool m_Stopped;
	};

} // namespace Chained

#define CH_PROFILE_BEGIN_SESSION(name, filepath)                                                                       \
	if (auto* _inst = ::Chained::Instrumentor::TryGet())                                                               \
	_inst->BeginSession(name, filepath)
#define CH_PROFILE_END_SESSION()                                                                                       \
	if (auto* _inst = ::Chained::Instrumentor::TryGet())                                                               \
	_inst->EndSession()
#define CH_PROFILE_CONCAT_IMPL(x, y) x##y
#define CH_PROFILE_CONCAT(x, y) CH_PROFILE_CONCAT_IMPL(x, y)
#define CH_PROFILE_SCOPE(name) ::Chained::InstrumentationTimer CH_PROFILE_CONCAT(timer, __LINE__)(name)
#define CH_PROFILE_FUNCTION() CH_PROFILE_SCOPE(__FUNCTION__)

#endif // CH_PROFILER_H
