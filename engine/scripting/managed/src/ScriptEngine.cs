using System;
using System.Collections.Generic;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Loader;

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
        private static Dictionary<string, Type> s_ScriptTypes = new Dictionary<string, Type>(StringComparer.OrdinalIgnoreCase);

        // Autoload (Global) scripts that persist across all scenes
        private static List<Script> s_GlobalScripts = new List<Script>();
        private static HashSet<Assembly> s_ScannedAssemblies = new HashSet<Assembly>();

        // AutoAttach mappings: Tag -> list of script Types
        private static Dictionary<string, List<Type>> s_AutoAttachByTag = new Dictionary<string, List<Type>>(StringComparer.OrdinalIgnoreCase);

        /// <summary>
        /// Scans all loaded assemblies across all AssemblyLoadContexts for [Autoload] and [AutoAttach] attributes.
        /// Runs dynamically so newly loaded assemblies (like GameScriptsALC) are immediately discovered.
        /// </summary>
        public static void EnsureTypesDiscovered()
        {
            var assembliesToScan = new List<Assembly>();

            try
            {
                foreach (var alc in AssemblyLoadContext.All)
                {
                    foreach (var asm in alc.Assemblies)
                    {
                        if (!assembliesToScan.Contains(asm))
                        {
                            assembliesToScan.Add(asm);
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                Log.Warn($"[ScriptEngine] Exception reading AssemblyLoadContext.All: {ex.Message}");
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
                if (!s_ScannedAssemblies.Add(asm))
                {
                    continue; // Already scanned this assembly
                }

                try
                {
                    foreach (var type in asm.GetTypes())
                    {
                        if (type.IsAbstract || !typeof(Script).IsAssignableFrom(type))
                            continue;

                        // Cache type by full name and short name
                        if (!string.IsNullOrEmpty(type.FullName))
                            s_ScriptTypes[type.FullName] = type;
                        if (!string.IsNullOrEmpty(type.Name))
                            s_ScriptTypes[type.Name] = type;

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
                                        Log.Info($"[ScriptEngine] Autoload script registered: {type.FullName}");
                                    }
                                }
                                catch (Exception ex)
                                {
                                    Log.Error($"[ScriptEngine] Failed to instantiate Autoload script '{type.FullName}':\n{ex}");
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
                                Log.Info($"[ScriptEngine] AutoAttach registered: Tag='{attr.Tag}' -> Script='{type.FullName}'");
                            }
                        }
                    }
                }
                catch (ReflectionTypeLoadException rtle)
                {
                    Log.Warn($"[ScriptEngine] Partial type load for assembly '{asm.GetName().Name}': {rtle.Message}");
                }
                catch (Exception ex)
                {
                    Log.Error($"[ScriptEngine] Exception scanning assembly '{asm.GetName().Name}':\n{ex}");
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
                Log.Error($"[ScriptEngine] Exception in ProcessAutoAttach:\n{ex}");
            }
        }

        /// <summary>
        /// Internal helper to instantiate and register a script for an entity.
        /// </summary>
        public static bool InstantiateScriptTypeInternal(ulong entityId, Type type)
        {
            if (type == null)
            {
                Log.Error("[ScriptEngine] Cannot instantiate script: Type is null.");
                return false;
            }

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
                    Log.Error($"[ScriptEngine] Failed to cast instance of '{type.FullName}' to Script.");
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
                Log.Error($"[ScriptEngine] Exception instantiating script '{type.FullName}' on Entity {entityId}:\n{ex}");
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
            if (string.IsNullOrEmpty(className))
            {
                Log.Warn("[ScriptEngine] InstantiateScript called with empty class name.");
                return 0;
            }

            EnsureTypesDiscovered();

            if (!s_ScriptTypes.TryGetValue(className, out Type? type))
            {
                foreach (var alc in AssemblyLoadContext.All)
                {
                    foreach (var assembly in alc.Assemblies)
                    {
                        type = assembly.GetType(className, false, true);
                        if (type != null) break;
                    }
                    if (type != null) break;
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
                    Log.Error($"[ScriptEngine] Could not find script type '{className}' across any loaded assembly.");
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
                    if (string.Equals(script.GetType().FullName, className, StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(script.GetType().Name, className, StringComparison.OrdinalIgnoreCase))
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
                                Log.Warn($"[ScriptEngine] Failed to set field '{fieldName}' on '{className}': {ex.Message}");
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
                    if (string.IsNullOrEmpty(className) ||
                        string.Equals(script.GetType().FullName, className, StringComparison.OrdinalIgnoreCase) ||
                        string.Equals(script.GetType().Name, className, StringComparison.OrdinalIgnoreCase))
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
                            Log.Error($"[ScriptEngine] Exception in OnDestroy for '{script.GetType().FullName}':\n{ex}");
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
                try
                {
                    script.OnDestroy();
                }
                catch (Exception ex)
                {
                    Log.Error($"[ScriptEngine] Exception in OnDestroy for '{script.GetType().FullName}':\n{ex}");
                }
            }
            foreach (var script in s_ScriptsNeedingCreate)
            {
                if (s_GlobalScripts.Contains(script)) continue;
                try
                {
                    script.OnDestroy();
                }
                catch (Exception ex)
                {
                    Log.Error($"[ScriptEngine] Exception in OnDestroy for '{script.GetType().FullName}':\n{ex}");
                }
            }
            foreach (var script in s_ScriptsNeedingStart)
            {
                if (s_GlobalScripts.Contains(script)) continue;
                try
                {
                    script.OnDestroy();
                }
                catch (Exception ex)
                {
                    Log.Error($"[ScriptEngine] Exception in OnDestroy for '{script.GetType().FullName}':\n{ex}");
                }
            }

            s_ActiveScripts.Clear();
            s_EntityScripts.Clear();
            s_ScriptsNeedingCreate.Clear();
            s_ScriptsNeedingStart.Clear();

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
                    Log.Error($"[ScriptEngine] Exception in OnSceneLoaded for '{globalScript.GetType().FullName}':\n{ex}");
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
            // Ensure Autoload / AutoAttach types are discovered from all loaded ALCs
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
                        // Script successfully created; queue for OnStart on next frame
                        s_ScriptsNeedingStart.Add(script);
                    }
                    catch (Exception ex)
                    {
                        Log.Error($"[ScriptEngine] Exception in OnCreate for '{script.GetType().FullName}' (Entity ID: {script.Entity?.ID}):\n{ex}");
                    }
                }
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
                        s_ActiveScripts.Add(script);
                    }
                    catch (Exception ex)
                    {
                        Log.Error($"[ScriptEngine] Exception in OnStart for '{script.GetType().FullName}' (Entity ID: {script.Entity?.ID}):\n{ex}");
                    }
                }
                s_ActiveScripts.Sort((a, b) => a.Priority.CompareTo(b.Priority));
            }

            // 3. OnUpdate for all active scripts
            for (int i = 0; i < s_ActiveScripts.Count; i++)
            {
                var script = s_ActiveScripts[i];
                try
                {
                    script.OnUpdate(deltaTime);
                }
                catch (Exception ex)
                {
                    Log.Error($"[ScriptEngine] Exception in OnUpdate for '{script.GetType().FullName}' (Entity ID: {script.Entity?.ID}):\n{ex}");
                }
            }
        }

        [UnmanagedCallersOnly]
        public static void OnEvent(int eventType)
        {
            for (int i = 0; i < s_ActiveScripts.Count; i++)
            {
                var script = s_ActiveScripts[i];
                script.__ResetEventState();
                try
                {
                    script.OnEvent(eventType);
                }
                catch (Exception ex)
                {
                    Log.Error($"[ScriptEngine] Exception in OnEvent for '{script.GetType().FullName}':\n{ex}");
                }
                if (script.EventConsumed) break;
            }
        }

        [UnmanagedCallersOnly]
        public static void OnRenderUI()
        {
            for (int i = 0; i < s_ActiveScripts.Count; i++)
            {
                var script = s_ActiveScripts[i];
                script.__ResetEventState();
                try
                {
                    script.OnGUI();
                }
                catch (Exception ex)
                {
                    Log.Error($"[ScriptEngine] Exception in OnGUI for '{script.GetType().FullName}':\n{ex}");
                }
                if (script.EventConsumed) break;
            }
        }

        [UnmanagedCallersOnly]
        public static void OnCollisionEnter(ulong entityA, ulong entityB)
        {
            if (s_EntityScripts.TryGetValue(entityA, out var scriptsA))
            {
                foreach (var script in scriptsA)
                {
                    try
                    {
                        script.OnCollisionEnter(entityB);
                    }
                    catch (Exception ex)
                    {
                        Log.Error($"[ScriptEngine] Exception in OnCollisionEnter for '{script.GetType().FullName}' (Entity {entityA}):\n{ex}");
                    }
                }
            }

            if (s_EntityScripts.TryGetValue(entityB, out var scriptsB))
            {
                foreach (var script in scriptsB)
                {
                    try
                    {
                        script.OnCollisionEnter(entityA);
                    }
                    catch (Exception ex)
                    {
                        Log.Error($"[ScriptEngine] Exception in OnCollisionEnter for '{script.GetType().FullName}' (Entity {entityB}):\n{ex}");
                    }
                }
            }
        }
    }
}
