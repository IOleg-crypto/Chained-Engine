using System;
using System.Reflection;
using System.Runtime.InteropServices;

namespace Chained
{
    /// <summary>
    /// Minimal interop: Coral fills the delegate* fields below via AddInternalCall
    /// (each maps to a C++ ScriptGlue_Register*Callback). C# then registers the
    /// managed lifecycle callbacks directly — no struct, no round-trip.
    /// </summary>
    public static unsafe class Interop
    {
        // Coral fills these via AddInternalCall (points to C++ ScriptGlue_Register*Callback).
        // Must be internal (non-public) — Coral uses BindingFlags.NonPublic to find delegate* fields.
        internal static delegate* unmanaged<delegate* unmanaged<float, void>, void> ScriptGlue_RegisterUpdateCallback_Ptr;
        internal static delegate* unmanaged<delegate* unmanaged<int, void>, void> ScriptGlue_RegisterEventCallback_Ptr;
        internal static delegate* unmanaged<delegate* unmanaged<void>, void> ScriptGlue_RegisterRenderUICallback_Ptr;
        internal static delegate* unmanaged<delegate* unmanaged<ulong, ulong, void>, void> ScriptGlue_RegisterCollisionCallback_Ptr;
        internal static delegate* unmanaged<delegate* unmanaged<void>, void> ScriptGlue_RegisterClearAllCallback_Ptr;
        internal static delegate* unmanaged<delegate* unmanaged<ulong, char*, byte>, void> ScriptGlue_RegisterInstantiateCallback_Ptr;
        internal static delegate* unmanaged<delegate* unmanaged<ulong, char*, void>, void> ScriptGlue_RegisterDestroyCallback_Ptr;
        internal static delegate* unmanaged<delegate* unmanaged<ulong, void>, void> ScriptGlue_RegisterSetSceneEpochCallback_Ptr;

        /// <summary>
        /// Called once from C++ after assemblies are loaded.
        /// NOT [UnmanagedCallersOnly] — invoked via managed InvokeStaticMethod.
        /// Registers each managed lifecycle callback with C++ directly.
        /// </summary>
        public static void RegisterCallbacks()
        {
            if (ScriptGlue_RegisterUpdateCallback_Ptr == null)
            {
                Console.WriteLine("[C# Interop] ERROR: ScriptGlue_Register*Callback fields are null - Coral did not fill them.");
                return;
            }

            ScriptGlue_RegisterUpdateCallback_Ptr(&ScriptEngine.OnUpdate);
            ScriptGlue_RegisterEventCallback_Ptr(&ScriptEngine.OnEvent);
            ScriptGlue_RegisterRenderUICallback_Ptr(&ScriptEngine.OnRenderUI);
            ScriptGlue_RegisterCollisionCallback_Ptr(&ScriptEngine.OnCollisionEnter);
            ScriptGlue_RegisterClearAllCallback_Ptr(&ScriptEngine.ClearAll);
            ScriptGlue_RegisterInstantiateCallback_Ptr(&ScriptEngine.InstantiateScript);
            ScriptGlue_RegisterDestroyCallback_Ptr(&ScriptEngine.DestroyScript);
            if (ScriptGlue_RegisterSetSceneEpochCallback_Ptr != null)
            {
                ScriptGlue_RegisterSetSceneEpochCallback_Ptr(&ScriptEngine.SetSceneEpoch);
            }

            Console.WriteLine("[C# Interop] Managed callbacks registered successfully.");
            VerifyInternalCalls();
        }

        /// <summary>
        /// Catches ABI/signature drift: any delegate* field suffixed with _Ptr that Coral
        /// failed to bind (name mismatch, signature mismatch) stays null. A null here means
        /// a C++ AddInternalCall has no matching C# field (or vice versa) — a silent crash
        /// waiting to happen. We surface it loudly at load time instead.
        /// </summary>
        private static void VerifyInternalCalls()
        {
            var unbound = new System.Collections.Generic.List<string>();
            var asm = typeof(Interop).Assembly;
            foreach (var type in asm.GetTypes())
            {
                foreach (var field in type.GetFields(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static))
                {
                    if (field.Name.EndsWith("_Ptr") && field.FieldType.IsFunctionPointer && field.GetValue(null) == null)
                        unbound.Add($"{type.Name}.{field.Name}");
                }
            }

            if (unbound.Count > 0)
                Console.WriteLine($"[C# Interop] ERROR: {unbound.Count} internal call(s) NOT bound by Coral: {string.Join(", ", unbound)}");
            else
                Console.WriteLine("[C# Interop] All internal calls bound successfully.");
        }
    }
}
