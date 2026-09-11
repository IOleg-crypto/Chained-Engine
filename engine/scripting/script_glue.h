// script_glue.h
// Registers C++ functions as Coral internal calls.
// Each function gets its own AddInternalCall — Coral fills the C# delegate* fields.
#ifndef CH_SCRIPT_GLUE_H
#define CH_SCRIPT_GLUE_H

namespace Coral
{
	class ManagedAssembly;
}

namespace Chained
{
	namespace ScriptGlue
	{
		// Registers all C++ ↔ C# interop functions with Coral.
		void RegisterInternalCalls(Coral::ManagedAssembly& assembly);
	} // namespace ScriptGlue
} // namespace Chained

#endif // CH_SCRIPT_GLUE_H
