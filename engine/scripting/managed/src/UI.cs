using Coral.Managed.Interop;

namespace Chained
{
    /// <summary>Immediate UI helpers for Script.OnGUI().</summary>
    public static class UI
    {
#pragma warning disable 0649
        internal static unsafe delegate* unmanaged<char*, void> UI_Text_Ptr;
        internal static unsafe delegate* unmanaged<char*, float, float, float, float, void> UI_TextColored_Ptr;
        internal static unsafe delegate* unmanaged<char*, byte> UI_Button_Ptr;
        internal static unsafe delegate* unmanaged<char*, float, float, float, float, float, void> UI_BeginWindow_Ptr;
        internal static unsafe delegate* unmanaged<void> UI_EndWindow_Ptr;
        internal static unsafe delegate* unmanaged<char*, char*, int, byte> UI_InputText_Ptr;
        internal static unsafe delegate* unmanaged<void> UI_SetKeyboardFocusHere_Ptr;
        internal static unsafe delegate* unmanaged<float, void> UI_SetScrollHereY_Ptr;
        internal static unsafe delegate* unmanaged<float*, float*, void> UI_GetDisplaySize_Ptr;
#pragma warning restore 0649

        /// <summary>Draws UI text.</summary>
        public static unsafe void Text(string text)
        {
            if (text == null || UI_Text_Ptr == null) return;
            fixed (char* ptr = text) UI_Text_Ptr(ptr);
        }

        /// <summary>Draws colored UI text.</summary>
        public static unsafe void TextColored(string text, float r, float g, float b, float a = 1.0f)
        {
            if (text == null || UI_TextColored_Ptr == null) return;
            fixed (char* ptr = text) UI_TextColored_Ptr(ptr, r, g, b, a);
        }

        /// <summary>Renders a button. Returns true when clicked.</summary>
        public static unsafe bool Button(string label)
        {
            if (label == null || UI_Button_Ptr == null) return false;
            fixed (char* ptr = label) return UI_Button_Ptr(ptr) != 0;
        }

        /// <summary>Begins a frameless window at the specified screen rectangle.</summary>
        public static unsafe void BeginWindow(string title, float x = -1.0f, float y = -1.0f, float width = -1.0f, float height = -1.0f, float bgAlpha = 0.6f)
        {
            if (title == null || UI_BeginWindow_Ptr == null) return;
            fixed (char* ptr = title) UI_BeginWindow_Ptr(ptr, x, y, width, height, bgAlpha);
        }

        /// <summary>Ends the current UI window.</summary>
        public static unsafe void EndWindow()
        {
            if (UI_EndWindow_Ptr == null) return;
            UI_EndWindow_Ptr();
        }

        /// <summary>
        /// Renders a text input box. Returns true if the user pressed Enter.
        /// </summary>
        public static unsafe bool InputText(string label, ref string text, int maxCapacity = 256)
        {
            if (UI_InputText_Ptr == null) return false;
            char[] buffer = new char[maxCapacity];
            if (!string.IsNullOrEmpty(text))
            {
                int len = System.Math.Min(text.Length, maxCapacity - 1);
                text.CopyTo(0, buffer, 0, len);
            }

            byte enterPressed = 0;
            fixed (char* lPtr = label)
            fixed (char* bPtr = buffer)
            {
                enterPressed = UI_InputText_Ptr(lPtr, bPtr, maxCapacity);
            }

            int end = 0;
            while (end < buffer.Length && buffer[end] != '\0') end++;
            text = new string(buffer, 0, end);
            return enterPressed != 0;
        }

        /// <summary>Sets keyboard focus on the next widget.</summary>
        public static unsafe void SetKeyboardFocusHere()
        {
            if (UI_SetKeyboardFocusHere_Ptr == null) return;
            UI_SetKeyboardFocusHere_Ptr();
        }

        /// <summary>Scrolls vertically to the bottom/ratio.</summary>
        public static unsafe void SetScrollHereY(float centerYRatio = 1.0f)
        {
            if (UI_SetScrollHereY_Ptr == null) return;
            UI_SetScrollHereY_Ptr(centerYRatio);
        }

        /// <summary>Returns the game viewport display size in pixels.</summary>
        public static unsafe Vector2 GetDisplaySize()
        {
            float w = 1280.0f, h = 720.0f;
            if (UI_GetDisplaySize_Ptr != null)
            {
                UI_GetDisplaySize_Ptr(&w, &h);
            }
            return new Vector2(w, h);
        }
    }
}