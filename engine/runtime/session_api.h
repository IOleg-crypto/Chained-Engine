
#ifndef CH_SESSION_API_H
#define CH_SESSION_API_H
#include <functional>

// Lightweight header — no heavy engine includes.
// RuntimeLayer populates these pointers in OnAttach() / destructor.
// engine_scripting includes only this file, breaking the circular dependency
// between engine_scripting <-> engine_runtime_core.

namespace Chained::SessionAPI
{
	// Returns true when a suspended gameplay session is waiting to be resumed.
	inline std::function<bool()> HasSuspendedSession;

	// Resumes the previously suspended gameplay session.
	inline std::function<void()> ResumeSuspendedSession;

	// Suspends the current gameplay scene and opens the start menu.
	// Deferred internally — safe to call from C# script update.
	inline std::function<void()> SuspendToMenu;

} // namespace Chained::SessionAPI

#endif // CH_SESSION_API_H