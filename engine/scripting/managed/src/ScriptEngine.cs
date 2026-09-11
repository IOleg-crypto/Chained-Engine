using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.InteropServices;

namespace Chained
{
    /// <summary>
    /// Central manager for all script instances.
    /// This is called from C++ once per frame, handling the lifecycle of all ManagedScripts natively in C#.
    /// </summary>
    public static class ScriptEngine
    {
        // Tracks all active scripts per entity ID.
        // It allows easy lookup of scripts attached to specific entities.
        private static Dictionary<ulong, List<Script>> s_EntityScripts = new Dictionary<ulong, List<Script>>();
        
        // A flat list of scripts for fast iteration during OnUpdate / OnGUI.
        private static List<Script> s_ActiveScripts = new List<Script>();
        
        // Scripts pending OnCreate() — deferred one frame to avoid re-entrant Coral calls
        private static List<Script> s_ScriptsNeedingCreate = new List<Script>();
        
        // Scripts that need their OnStart called (one frame after OnCreate)
        private static List<Script> s_ScriptsNeedingStart = new List<Script>();
        
        // Caches script types to avoid reflection overhead on every instantiation
        private static Dictionary<string, Type> s_ScriptTypes = new Dictionary<string, Type>();

        /// <summary>
        /// Called from C++ when a script component is encountered.
        /// IMPORTANT: Does NOT call OnCreate here to avoid re-entrant Coral calls.
        /// OnCreate is deferred to the next OnUpdate tick.
        /// </summary>
        [UnmanagedCallersOnly]
        public static unsafe byte InstantiateScript(ulong entityId, char* classNamePtr)
        {
            string className = (classNamePtr != null) ? new string(classNamePtr) : string.Empty;
            if (!s_ScriptTypes.TryGetValue(className, out Type? type))
            {
                // Search only within the active AssemblyLoadContext to avoid resolving 
                // stale types from old, unloaded contexts pending garbage collection.
                var activeALC = System.Runtime.Loader.AssemblyLoadContext.GetLoadContext(typeof(ScriptEngine).Assembly);
                if (activeALC != null)
                {
                    foreach (var assembly in activeALC.Assemblies)
                    {
                        type = assembly.GetType(className, false, true);
                        if (type != null) break;
                    }
                }
                
                // Fallback for default context (e.g. system types)
                if (type == null)
                {
                    foreach (var assembly in AppDomain.CurrentDomain.GetAssemblies())
                    {
                        type = assembly.GetType(className, false, true);
                        if (type != null) break;
                    }
                }

                if (type == null)
                {
                    Console.WriteLine($"[ScriptEngine] ERROR: Could not find script type: {className}");
                    return 0;
                }
                s_ScriptTypes[className] = type;
            }

            try
            {
                // Check if an instance of this type is already registered for this entity
                // BEFORE creating the object to avoid wasted allocation.
                if (s_EntityScripts.TryGetValue(entityId, out var existingList))
                {
                    foreach (var existing in existingList)
                    {
                        if (existing.GetType() == type)
                        {
                            return 1;
                        }
                    }
                }

                Script? script = Activator.CreateInstance(type) as Script;
                if (script == null)
                {
                    Console.WriteLine($"[ScriptEngine] ERROR: Failed to cast {className} to Script.");
                    return 0;
                }

                script.__Init(entityId);

                if (!s_EntityScripts.TryGetValue(entityId, out var scriptList))
                {
                    scriptList = new List<Script>();
                    s_EntityScripts[entityId] = scriptList;
                }
                scriptList.Add(script);
                
                // Defer OnCreate to next OnUpdate to avoid re-entrant Coral calls.
                // Do NOT add to s_ActiveScripts yet — the script graduates there
                // only after OnStart completes, so OnUpdate never runs prematurely.
                s_ScriptsNeedingCreate.Add(script);
                return 1;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[ScriptEngine] ERROR: Exception instantiating {className}: {ex}");
                return 0;
            }
        }

        private static void InternalSetField(ulong entityId, string className, string fieldName, object value)
        {
            if (s_EntityScripts.TryGetValue(entityId, out var scriptList))
            {
                foreach (var script in scriptList)
                {
                    if (script.GetType().FullName == className || script.GetType().Name == className)
                    {
                        var field = script.GetType().GetField(fieldName, BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance);
                        if (field != null)
                        {
                            try
                            {
                                field.SetValue(script, Convert.ChangeType(value, field.FieldType));
                            }
                            catch (Exception ex)
                            {
                                Console.WriteLine($"[ScriptEngine] WARN: Failed to set field {fieldName} on {className}: {ex.Message}");
                            }
                        }
                        return;
                    }
                }
            }
        }

        public static void SetFieldFloat(ulong entityId, string className, string fieldName, float value) => InternalSetField(entityId, className, fieldName, value);
        public static void SetFieldInt(ulong entityId, string className, string fieldName, int value) => InternalSetField(entityId, className, fieldName, value);
        public static void SetFieldBool(ulong entityId, string className, string fieldName, bool value) => InternalSetField(entityId, className, fieldName, value);
        public static void SetFieldString(ulong entityId, string className, string fieldName, string value) => InternalSetField(entityId, className, fieldName, value);
        // Using objects for math types as they might require custom marshaling depending on how Coral works.


        [UnmanagedCallersOnly]
        public static unsafe void DestroyScript(ulong entityId, char* classNamePtr)
        {
            string className = (classNamePtr != null) ? new string(classNamePtr) : string.Empty;
            DestroyScriptInternal(entityId, className);
        }

        private static void DestroyScriptInternal(ulong entityId, string className)
        {
            if (s_EntityScripts.TryGetValue(entityId, out var scriptList))
            {
                for (int i = 0; i < scriptList.Count; i++)
                {
                    var script = scriptList[i];
                    if (string.IsNullOrEmpty(className) || script.GetType().FullName == className || script.GetType().Name == className)
                    {
                        try
                        {
                            script.OnDestroy();
                        }
                        catch (Exception ex)
                        {
                            Console.WriteLine($"[ScriptEngine] ERROR: Exception destroying {className}: {ex.Message}");
                        }
                        
                        s_ActiveScripts.Remove(script);
                        s_ScriptsNeedingCreate.Remove(script);
                        s_ScriptsNeedingStart.Remove(script);
                        scriptList.RemoveAt(i);
                        i--;
                    }
                }

                if (scriptList.Count == 0)
                {
                    s_EntityScripts.Remove(entityId);
                }
            }
        }

        [UnmanagedCallersOnly]
        public static void ClearAll()
        {
            foreach (var script in s_ActiveScripts)
            {
                try { script.OnDestroy(); }
                catch (Exception ex)
                {
                    Console.WriteLine($"[ScriptEngine] ERROR: Exception in OnDestroy for {script.GetType().Name}: {ex.Message}");
                }
            }
            foreach (var script in s_ScriptsNeedingCreate)
            {
                try { script.OnDestroy(); }
                catch (Exception ex)
                {
                    Console.WriteLine($"[ScriptEngine] ERROR: Exception in OnDestroy for {script.GetType().Name}: {ex.Message}");
                }
            }
            foreach (var script in s_ScriptsNeedingStart)
            {
                try { script.OnDestroy(); }
                catch (Exception ex)
                {
                    Console.WriteLine($"[ScriptEngine] ERROR: Exception in OnDestroy for {script.GetType().Name}: {ex.Message}");
                }
            }
            s_ActiveScripts.Clear();
            s_EntityScripts.Clear();
            s_ScriptsNeedingCreate.Clear();
            s_ScriptsNeedingStart.Clear();
            s_ScriptTypes.Clear();
        }

        [UnmanagedCallersOnly]
        public static void OnUpdate(float deltaTime)
        {
            // 1. OnCreate for newly instantiated scripts
            if (s_ScriptsNeedingCreate.Count > 0)
            {
                var batch = new List<Script>(s_ScriptsNeedingCreate);
                s_ScriptsNeedingCreate.Clear();
                foreach (var script in batch)
                {
                    try
                    {
                        script.OnCreate();
                    }
                    catch (Exception ex)
                    {
                        Console.WriteLine($"[ScriptEngine] ERROR: Exception in OnCreate for {script.GetType().Name}: {ex.Message}");
                    }
                }
                // They need OnStart on the NEXT frame
                s_ScriptsNeedingStart.AddRange(batch);
            }

            // 2. OnStart for scripts that got OnCreate last frame
            if (s_ScriptsNeedingStart.Count > 0)
            {
                var batch = new List<Script>(s_ScriptsNeedingStart);
                s_ScriptsNeedingStart.Clear();
                foreach (var script in batch)
                {
                    try
                    {
                        script.OnStart();
                    }
                    catch (Exception ex)
                    {
                        Console.WriteLine($"[ScriptEngine] ERROR: Exception in OnStart for {script.GetType().Name}: {ex.Message}");
                    }
                    // Graduate to active only after OnStart — prevents OnUpdate from
                    // running on the same frame as OnCreate (which caused button auto-fire).
                    s_ActiveScripts.Add(script);
                }
                // Re-sort by priority so newly added scripts interleave correctly
                s_ActiveScripts.Sort((a, b) => a.Priority.CompareTo(b.Priority));
            }

            // 3. OnUpdate for all active scripts
            for (int i = 0; i < s_ActiveScripts.Count; i++)
            {
                try
                {
                    s_ActiveScripts[i].OnUpdate(deltaTime);
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"[ScriptEngine] ERROR: Exception in OnUpdate for {s_ActiveScripts[i].GetType().Name}: {ex.Message}");
                }
            }
        }

        [UnmanagedCallersOnly]
        public static void OnEvent(int eventType)
        {
            for (int i = 0; i < s_ActiveScripts.Count; i++)
            {
                s_ActiveScripts[i].__ResetEventState();
                try
                {
                    s_ActiveScripts[i].OnEvent(eventType);
                }
                catch (Exception ex)
                {
                     Console.WriteLine($"[ScriptEngine] ERROR: Exception in OnEvent for {s_ActiveScripts[i].GetType().Name}: {ex.Message}");
                }
                if (s_ActiveScripts[i].EventConsumed) break;
            }
        }

        [UnmanagedCallersOnly]
        public static void OnRenderUI()
        {
            for (int i = 0; i < s_ActiveScripts.Count; i++)
            {
                s_ActiveScripts[i].__ResetEventState();
                try
                {
                    s_ActiveScripts[i].OnGUI();
                }
                catch (Exception ex)
                {
                     Console.WriteLine($"[ScriptEngine] ERROR: Exception in OnGUI for {s_ActiveScripts[i].GetType().Name}: {ex.Message}");
                }
                if (s_ActiveScripts[i].EventConsumed) break;
            }
        }
        // Some func not work
        [UnmanagedCallersOnly]
        public static void OnCollisionEnter(ulong entityA, ulong entityB)
        {
            // Dispatch to entity A
            if (s_EntityScripts.TryGetValue(entityA, out var scriptsA))
            {
                foreach(var script in scriptsA)
                {
                    try { script.OnCollisionEnter(entityB); }
                    catch (Exception ex)
                    {
                        Console.WriteLine($"[ScriptEngine] ERROR: Exception in OnCollisionEnter for {script.GetType().Name}: {ex.Message}");
                    }
                }
            }

            // Dispatch to entity B
            if (s_EntityScripts.TryGetValue(entityB, out var scriptsB))
            {
                foreach(var script in scriptsB)
                {
                    try { script.OnCollisionEnter(entityA); }
                    catch (Exception ex)
                    {
                        Console.WriteLine($"[ScriptEngine] ERROR: Exception in OnCollisionEnter for {script.GetType().Name}: {ex.Message}");
                    }
                }
            }
        }
    }
}
