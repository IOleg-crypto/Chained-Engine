#ifndef CH_SESSION_API_H
#define CH_SESSION_API_H

// Lightweight header — no heavy engine includes.
// RuntimeLayer populates these pointers in OnAttach() / destructor.
// engine_scripting includes only this file, breaking the circular dependency
// between engine_scripting <-> engine_runtime_core.

namespace Chained::SessionAPI
{
	// Returns true when a suspended gameplay session is waiting to be resumed.
	inline bool (*HasSuspendedSession)() = nullptr;

	// Resumes the previously suspended gameplay session.
	inline void (*ResumeSuspendedSession)() = nullptr;

} // namespace Chained::SessionAPI

#endif // CH_SESSION_API_H