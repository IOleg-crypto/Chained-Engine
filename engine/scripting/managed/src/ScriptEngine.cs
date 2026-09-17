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

        // Autoload (Global) scripts that persist across all scenes
        private static List<Script> s_GlobalScripts = new List<Script>();
        private static bool s_AutoloadScanned = false;

        // AutoAttach mappings: Tag -> list of script Types
        private static Dictionary<string, List<Type>> s_AutoAttachByTag = new Dictionary<string, List<Type>>(StringComparer.OrdinalIgnoreCase);

        /// <summary>
        /// Scans all loaded assemblies for [Autoload] and [AutoAttach] attributes.
        /// </summary>
        public static void EnsureTypesDiscovered()
        {
            if (s_AutoloadScanned) return;
            s_AutoloadScanned = true;

            var assembliesToScan = new List<Assembly>();
            var activeALC = System.Runtime.Loader.AssemblyLoadContext.GetLoadContext(typeof(ScriptEngine).Assembly);
            if (activeALC != null)
            {
                assembliesToScan.AddRange(activeALC.Assemblies);
            }
            foreach (var asm in AppDomain.CurrentDomain.GetAssemblies())
            {
                if (!assembliesToScan.Contains(asm))
                {
                    assembliesToScan.Add(asm);
                }
            }

            foreach (var asm in assembliesToScan)
            {
                try
                {
                    foreach (var type in asm.GetTypes())
                    {
                        if (type.IsAbstract || !typeof(Script).IsAssignableFrom(type))
                            continue;

                        // 1. Check Autoload / GlobalScript
                        bool isAutoload = type.IsDefined(typeof(AutoloadAttribute), true) ||
                                          type.IsDefined(typeof(GlobalScriptAttribute), true);
                        if (isAutoload)
                        {
                            bool alreadyCreated = s_GlobalScripts.Exists(s => s.GetType() == type);
                            if (!alreadyCreated)
                            {
                                try
                                {
                                    if (Activator.CreateInstance(type) is Script globalInst)
                                    {
                                        globalInst.__Init(0); // Global entity ID
                                        s_GlobalScripts.Add(globalInst);
                                        s_ScriptsNeedingCreate.Add(globalInst);
                                        Console.WriteLine($"[ScriptEngine] Autoload script initialized: {type.FullName}");
                                    }
                                }
                                catch (Exception ex)
                                {
                                    Console.WriteLine($"[ScriptEngine] ERROR: Failed to instantiate Autoload script {type.FullName}: {ex.Message}");
                                }
                            }
                        }

                        // 2. Check AutoAttach
                        var autoAttachAttrs = type.GetCustomAttributes<AutoAttachAttribute>(true);
                        foreach (var attr in autoAttachAttrs)
                        {
                            if (string.IsNullOrEmpty(attr.Tag)) continue;
                            if (!s_AutoAttachByTag.TryGetValue(attr.Tag, out var list))
                            {
                                list = new List<Type>();
                                s_AutoAttachByTag[attr.Tag] = list;
                            }
                            if (!list.Contains(type))
                            {
                                list.Add(type);
                                Console.WriteLine($"[ScriptEngine] Registered AutoAttach: Tag='{attr.Tag}' -> Script='{type.Name}'");
                            }
                        }
                    }
                }
                catch (ReflectionTypeLoadException)
                {
                    // Skip assemblies with missing dependencies safely
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"[ScriptEngine] WARN: Exception scanning assembly {asm.GetName().Name}: {ex.Message}");
                }
            }
        }

        /// <summary>
        /// Automatically attaches scripts to entities with matching tags in the current scene.
        /// </summary>
        public static void ProcessAutoAttach()
        {
            if (s_AutoAttachByTag.Count == 0) return;

            try
            {
                ulong[] taggedEntities = Entity.FindAllWithComponent<TagComponent>();
                if (taggedEntities == null || taggedEntities.Length == 0) return;

                foreach (ulong entityId in taggedEntities)
                {
                    var entity = new Entity(entityId);
                    var tagComp = entity.GetComponent<TagComponent>();
                    if (tagComp == null || string.IsNullOrEmpty(tagComp.Tag)) continue;

                    if (s_AutoAttachByTag.TryGetValue(tagComp.Tag, out var scriptTypes))
                    {
                        foreach (var type in scriptTypes)
                        {
                            InstantiateScriptTypeInternal(entityId, type);
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[ScriptEngine] WARN: Exception in ProcessAutoAttach: {ex.Message}");
            }
        }

        /// <summary>
        /// Internal helper to instantiate and register a script for an entity.
        /// </summary>
        public static bool InstantiateScriptTypeInternal(ulong entityId, Type type)
        {
            if (s_EntityScripts.TryGetValue(entityId, out var existingList))
            {
                foreach (var existing in existingList)
                {
                    if (existing.GetType() == type)
                    {
                        return true; // Already exists
                    }
                }
            }

            try
            {
                Script? script = Activator.CreateInstance(type) as Script;
                if (script == null)
                {
                    Console.WriteLine($"[ScriptEngine] ERROR: Failed to cast {type.FullName} to Script.");
                    return false;
                }

                script.__Init(entityId);

                if (!s_EntityScripts.TryGetValue(entityId, out var scriptList))
                {
                    scriptList = new List<Script>();
                    s_EntityScripts[entityId] = scriptList;
                }
                scriptList.Add(script);

                s_ScriptsNeedingCreate.Add(script);
                return true;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[ScriptEngine] ERROR: Exception instantiating {type.FullName}: {ex}");
                return false;
            }
        }

        /// <summary>
        /// Native export called from C++ when a script component is encountered.
        /// </summary>
        [UnmanagedCallersOnly]
        public static unsafe byte InstantiateScript(ulong entityId, char* classNamePtr)
        {
            string className = (classNamePtr != null) ? new string(classNamePtr) : string.Empty;
            return InstantiateScriptManaged(entityId, className);
        }

        /// <summary>
        /// Managed method to instantiate a script by class name for an entity.
        /// </summary>
        public static byte InstantiateScriptManaged(ulong entityId, string className)
        {
            if (!s_ScriptTypes.TryGetValue(className, out Type? type))
            {
                var activeALC = System.Runtime.Loader.AssemblyLoadContext.GetLoadContext(typeof(ScriptEngine).Assembly);
                if (activeALC != null)
                {
                    foreach (var assembly in activeALC.Assemblies)
                    {
                        type = assembly.GetType(className, false, true);
                        if (type != null) break;
                    }
                }
                
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

            return (byte)(InstantiateScriptTypeInternal(entityId, type) ? 1 : 0);
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
        public static void SetFieldVector2(ulong entityId, string className, string fieldName, float x, float y) => InternalSetField(entityId, className, fieldName, new Vector2(x, y));
        public static void SetFieldVector3(ulong entityId, string className, string fieldName, float x, float y, float z) => InternalSetField(entityId, className, fieldName, new Vector3(x, y, z));
        public static void SetFieldVector4(ulong entityId, string className, string fieldName, float x, float y, float z, float w) => InternalSetField(entityId, className, fieldName, new Vector4(x, y, z, w));
        public static void SetFieldEntity(ulong entityId, string className, string fieldName, ulong targetEntityId) => InternalSetField(entityId, className, fieldName, new Entity(targetEntityId));

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
                        // Protect global scripts from being destroyed during regular entity cleanups
                        if (s_GlobalScripts.Contains(script))
                        {
                            continue;
                        }

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
        public static void ClearAll() => ClearAllManaged();

        /// <summary>
        /// Managed version of ClearAll for scene transitions and tests.
        /// </summary>
        public static void ClearAllManaged()
        {
            // Destroy only scene-bound scripts; preserve Autoload (Global) scripts
            foreach (var script in s_ActiveScripts)
            {
                if (s_GlobalScripts.Contains(script)) continue;
                try { script.OnDestroy(); }
                catch (Exception ex)
                {
                    Console.WriteLine($"[ScriptEngine] ERROR: Exception in OnDestroy for {script.GetType().Name}: {ex.Message}");
                }
            }
            foreach (var script in s_ScriptsNeedingCreate)
            {
                if (s_GlobalScripts.Contains(script)) continue;
                try { script.OnDestroy(); }
                catch (Exception ex)
                {
                    Console.WriteLine($"[ScriptEngine] ERROR: Exception in OnDestroy for {script.GetType().Name}: {ex.Message}");
                }
            }
            foreach (var script in s_ScriptsNeedingStart)
            {
                if (s_GlobalScripts.Contains(script)) continue;
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

            // Re-register surviving Autoload global scripts and notify them of scene reload
            foreach (var globalScript in s_GlobalScripts)
            {
                s_ActiveScripts.Add(globalScript);
                ulong gid = globalScript.Entity.ID;
                if (!s_EntityScripts.ContainsKey(gid))
                {
                    s_EntityScripts[gid] = new List<Script>();
                }
                s_EntityScripts[gid].Add(globalScript);

                try
                {
                    globalScript.OnSceneLoaded();
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"[ScriptEngine] ERROR: Exception in OnSceneLoaded for {globalScript.GetType().Name}: {ex.Message}");
                }
            }
        }

        [UnmanagedCallersOnly]
        public static void OnUpdate(float deltaTime) => OnUpdateManaged(deltaTime);

        /// <summary>
        /// Managed version of OnUpdate for game loop and tests.
        /// </summary>
        public static void OnUpdateManaged(float deltaTime)
        {
            // Ensure Autoload / AutoAttach types are discovered
            EnsureTypesDiscovered();

            // Process automatic tag-based attachment for newly loaded entities
            ProcessAutoAttach();

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
                    s_ActiveScripts.Add(script);
                }
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

        [UnmanagedCallersOnly]
        public static void OnCollisionEnter(ulong entityA, ulong entityB)
        {
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
