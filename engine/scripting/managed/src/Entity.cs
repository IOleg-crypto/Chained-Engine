using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using Coral.Managed.Interop;

namespace Chained
{


// ─────────────────────────────────────────────────────────────────────────────
//  Entity — wraps a native entity ID and provides component access
// ─────────────────────────────────────────────────────────────────────────────
/// <summary>Entity wrapper with a small component cache.</summary>
public class Entity
{
    /// <summary>Native entity handle.</summary>
    public ulong ID { get; private set; }

    // Cache: avoids allocating a new stub on every GetComponent<T>() call
    private readonly Dictionary<System.Type, Component> _cache = new();

#pragma warning disable 0649
    internal static unsafe delegate* unmanaged<ulong, char*, byte> Entity_HasComponent_Ptr;
    internal static unsafe delegate* unmanaged<char*, ulong*, int, int> Entity_FindAllWithComponent_Ptr;
    internal static unsafe delegate* unmanaged<ulong, char*, void> Entity_AddComponent_Ptr;
#pragma warning restore 0649

    public const ulong NullID = 0xFFFFFFFF;

    /// <summary>Wraps a native entity ID.</summary>
    public Entity(ulong id) { ID = id; }

    /// <summary>True when the entity handle is valid.</summary>
    public bool IsValid => ID != NullID && ID != ulong.MaxValue && ID != (ulong)uint.MaxValue;

    /// <summary>Quick access to the TransformComponent, or null if absent.</summary>
    public TransformComponent? Transform => GetComponent<TransformComponent>();

    // ── Component access ──────────────────────────────────────────────────

    private static unsafe bool HasComponent_Native(ulong entityID, string componentName)
    {
        if (Entity_HasComponent_Ptr == null) return false;
        fixed (char* ptr = componentName)
            return Entity_HasComponent_Ptr(entityID, ptr) != 0;
    }

    /// <summary>True when the component exists.</summary>
    public bool HasComponent<T>() where T : Component, new()
        => IsValid && HasComponent_Native(ID, typeof(T).Name);

    /// <summary>Returns the component wrapper, or null.</summary>
    public T? GetComponent<T>() where T : Component, new()
    {
        if (!IsValid) return null;

        System.Type componentType = typeof(T);
        if (_cache.TryGetValue(componentType, out Component? cachedComponent))
            return (T)cachedComponent;

        if (!HasComponent<T>())
            return null;

        T component = new T() { Entity = this };
        _cache[componentType] = component;
        return component;
    }

    /// <summary>Adds a component and returns its wrapper.</summary>
    public T AddComponent<T>() where T : Component, new()
    {
        if (!IsValid) throw new System.Exception("Cannot add component to invalid entity.");
        string name = typeof(T).Name;
        unsafe
        {
            if (Entity_AddComponent_Ptr == null) return new T() { Entity = this };
            fixed (char* ptr = name) Entity_AddComponent_Ptr(ID, ptr);
        }
        T component = new T() { Entity = this };
        _cache[typeof(T)] = component;
        return component;
    }

    /// <summary>Returns all entity IDs with the component.</summary>
    public static unsafe ulong[] FindAllWithComponent<T>() where T : Component, new()
    {
        if (Entity_FindAllWithComponent_Ptr == null) return Array.Empty<ulong>();
        string name = typeof(T).Name;
        fixed (char* ptr = name)
        {
            ulong* buf = stackalloc ulong[512];
            int totalCount = Entity_FindAllWithComponent_Ptr(ptr, buf, 512);
            if (totalCount <= 0) return Array.Empty<ulong>();

            if (totalCount <= 512)
            {
                ulong[] result = new ulong[totalCount];
                for (int i = 0; i < totalCount; i++) result[i] = buf[i];
                return result;
            }

            ulong[] dynamicResult = new ulong[totalCount];
            fixed (ulong* dynPtr = dynamicResult)
            {
                Entity_FindAllWithComponent_Ptr(ptr, dynPtr, totalCount);
            }
            return dynamicResult;
        }
    }

    /// <summary>Clears the component cache.</summary>
    public void InvalidateComponentCache() => _cache.Clear();

    public override string ToString() => $"Entity({ID})";
}

}

