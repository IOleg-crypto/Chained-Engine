using System;

namespace Chained
{
    [NativeCall("Chained.Input", "Input_IsKeyDown", "int", "int")]
    [NativeCall("Chained.Input", "Input_IsKeyPressed", "int", "int")]
    [NativeCall("Chained.Input", "Input_IsKeyReleased", "int", "int")]
    [NativeCall("Chained.Input", "Input_IsMouseButtonDown", "int", "int")]
    [NativeCall("Chained.Input", "Input_IsMouseButtonPressed", "int", "int")]
    [NativeCall("Chained.Input", "Input_GetMouseWheelMove", "float")]
    [NativeCall("Chained.Input", "Input_GetMouseWheelHMove", "float")]
    [NativeCall("Chained.Input", "Input_GetMouseScroll", "void", "float*", "float*")]
    [NativeCall("Chained.Input", "Input_GetMouseDelta", "void", "float*", "float*")]
    [NativeCall("Chained.Input", "Input_GetMousePosition", "void", "float*", "float*")]
    public static unsafe partial class Input
    {
        public static bool IsKeyDown(Key keyCode)
        {
            if (Input_IsKeyDown_Ptr == null) return false;
            return Input_IsKeyDown_Ptr((int)keyCode) != 0;
        }

        public static bool IsKeyPressed(Key keyCode)
        {
            if (Input_IsKeyPressed_Ptr == null) return false;
            return Input_IsKeyPressed_Ptr((int)keyCode) != 0;
        }

        public static bool IsKeyReleased(Key keyCode)
        {
            if (Input_IsKeyReleased_Ptr == null) return false;
            return Input_IsKeyReleased_Ptr((int)keyCode) != 0;
        }

        public static bool IsMouseButtonDown(MouseButton button)
        {
            if (Input_IsMouseButtonDown_Ptr == null) return false;
            return Input_IsMouseButtonDown_Ptr((int)button) != 0;
        }

        public static bool IsMouseButtonPressed(MouseButton button)
        {
            if (Input_IsMouseButtonPressed_Ptr == null) return false;
            return Input_IsMouseButtonPressed_Ptr((int)button) != 0;
        }

        public static float GetMouseWheelMove()
        {
            if (Input_GetMouseWheelMove_Ptr == null) return 0.0f;
            return Input_GetMouseWheelMove_Ptr();
        }

        public static float GetMouseWheelHMove()
        {
            if (Input_GetMouseWheelHMove_Ptr == null) return 0.0f;
            return Input_GetMouseWheelHMove_Ptr();
        }

        public static Vector2 MouseScroll
        {
            get
            {
                if (Input_GetMouseScroll_Ptr == null) return Vector2.Zero;
                float x = 0.0f, y = 0.0f;
                Input_GetMouseScroll_Ptr(&x, &y);
                return new Vector2(x, y);
            }
        }

        public static Vector3 MouseDelta
        {
            get
            {
                if (Input_GetMouseDelta_Ptr == null) return Vector3.Zero;
                float x = 0.0f, y = 0.0f;
                Input_GetMouseDelta_Ptr(&x, &y);
                return new Vector3(x, y, 0.0f);
            }
        }
        
        public static Vector2 MousePosition
        {
            get
            {
                if (Input_GetMousePosition_Ptr == null) return Vector2.Zero;
                float x = 0.0f, y = 0.0f;
                Input_GetMousePosition_Ptr(&x, &y);
                return new Vector2(x, y);
            }
        }
    }
}
