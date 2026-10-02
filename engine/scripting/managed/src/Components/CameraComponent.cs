using System;
using System.Runtime.InteropServices;

namespace Chained
{
    /// <summary>Camera wrapper.</summary>
    [NativeProperty("Primary", "bool", "Camera_GetPrimary", "Camera_SetPrimary")]
    [NativeProperty("IsOrbitCamera", "bool", "Camera_GetIsOrbit", "Camera_SetIsOrbit")]
    [NativeProperty("TargetEntityTag", "string", "Camera_GetTargetTag", "Camera_SetTargetTag")]
    [NativeCall("Chained.CameraComponent", "Camera_GetForward", "void", "ulong", "Vector3*")]
    [NativeCall("Chained.CameraComponent", "Camera_GetRight", "void", "ulong", "Vector3*")]
    [NativeCall("Chained.CameraComponent", "Camera_GetOrbit", "void", "ulong", "float*", "float*", "float*")]
    [NativeCall("Chained.CameraComponent", "Camera_SetOrbit", "void", "ulong", "float", "float", "float")]
    public partial class CameraComponent : Component
    {
        public Vector3 Forward
        {
            get
            {
                unsafe
                {
                    Vector3 val = default;
                    if (Camera_GetForward_Ptr != null) Camera_GetForward_Ptr(Entity.ID, &val);
                    return val;
                }
            }
        }

        public Vector3 Right
        {
            get
            {
                unsafe
                {
                    Vector3 val = default;
                    if (Camera_GetRight_Ptr != null) Camera_GetRight_Ptr(Entity.ID, &val);
                    return val;
                }
            }
        }

        public void GetOrbit(out float yaw, out float pitch, out float distance)
        {
            unsafe
            {
                float y = 0, p = 0, d = 0;
                if (Camera_GetOrbit_Ptr != null) Camera_GetOrbit_Ptr(Entity.ID, &y, &p, &d);
                yaw = y; pitch = p; distance = d;
            }
        }

        public void SetOrbit(float yaw, float pitch, float distance)
        {
            unsafe { if (Camera_SetOrbit_Ptr != null) Camera_SetOrbit_Ptr(Entity.ID, yaw, pitch, distance); }
        }
    }
}
