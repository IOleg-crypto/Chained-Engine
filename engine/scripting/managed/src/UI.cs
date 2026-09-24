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
        internal static unsafe delegate* unmanaged<char*, float, float, byte, void> UI_BeginChild_Ptr;
        internal static unsafe delegate* unmanaged<void> UI_EndChild_Ptr;
        internal static unsafe delegate* unmanaged<void> UI_Separator_Ptr;
        internal static unsafe delegate* unmanaged<float, float, void> UI_SameLine_Ptr;
        internal static unsafe delegate* unmanaged<float, void> UI_SetNextItemWidth_Ptr;
        internal static unsafe delegate* unmanaged<char*, char*, int, byte> UI_InputText_Ptr;
        internal static unsafe delegate* unmanaged<void> UI_SetKeyboardFocusHere_Ptr;
        internal static unsafe delegate* unmanaged<float, void> UI_SetScrollHereY_Ptr;
        internal static unsafe delegate* unmanaged<float*, float*, void> UI_GetDisplaySize_Ptr;
        internal static unsafe delegate* unmanaged<int, float, float, float, float, void> UI_PushStyleColor_Ptr;
        internal static unsafe delegate* unmanaged<int, void> UI_PopStyleColor_Ptr;
        internal static unsafe delegate* unmanaged<int, float, void> UI_PushStyleVarFloat_Ptr;
        internal static unsafe delegate* unmanaged<int, float, float, void> UI_PushStyleVarVec2_Ptr;
        internal static unsafe delegate* unmanaged<int, void> UI_PopStyleVar_Ptr;
        internal static unsafe delegate* unmanaged<float, float, void> UI_Dummy_Ptr;
        internal static unsafe delegate* unmanaged<byte> UI_IsItemHovered_Ptr;
        internal static unsafe delegate* unmanaged<float, void> UI_SetWindowFontScale_Ptr;
        internal static unsafe delegate* unmanaged<char*, float*, float, float, byte> UI_SliderFloat_Ptr;
        internal static unsafe delegate* unmanaged<char*, byte*, byte> UI_Checkbox_Ptr;
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

        /// <summary>Begins a scrollable child region.</summary>
        public static unsafe void BeginChild(string strId, float width = 0.0f, float height = 0.0f, bool border = false)
        {
            if (strId == null || UI_BeginChild_Ptr == null) return;
            fixed (char* ptr = strId) UI_BeginChild_Ptr(ptr, width, height, (byte)(border ? 1 : 0));
        }

        /// <summary>Ends the current scrollable child region.</summary>
        public static unsafe void EndChild()
        {
            if (UI_EndChild_Ptr == null) return;
            UI_EndChild_Ptr();
        }

        /// <summary>Draws a horizontal separator line.</summary>
        public static unsafe void Separator()
        {
            if (UI_Separator_Ptr == null) return;
            UI_Separator_Ptr();
        }

        /// <summary>Positions the next widget on the same line.</summary>
        public static unsafe void SameLine(float offsetFromStartX = 0.0f, float spacing = -1.0f)
        {
            if (UI_SameLine_Ptr == null) return;
            UI_SameLine_Ptr(offsetFromStartX, spacing);
        }

        /// <summary>Sets the width of the next widget.</summary>
        public static unsafe void SetNextItemWidth(float itemWidth)
        {
            if (UI_SetNextItemWidth_Ptr == null) return;
            UI_SetNextItemWidth_Ptr(itemWidth);
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

        /// <summary>Pushes a style color override.</summary>
        public static unsafe void PushStyleColor(int colIdx, float r, float g, float b, float a = 1.0f)
        {
            if (UI_PushStyleColor_Ptr == null) return;
            UI_PushStyleColor_Ptr(colIdx, r, g, b, a);
        }

        /// <summary>Pops style color overrides.</summary>
        public static unsafe void PopStyleColor(int count = 1)
        {
            if (UI_PopStyleColor_Ptr == null) return;
            UI_PopStyleColor_Ptr(count);
        }

        /// <summary>Pushes a float style variable.</summary>
        public static unsafe void PushStyleVar(int varIdx, float val)
        {
            if (UI_PushStyleVarFloat_Ptr == null) return;
            UI_PushStyleVarFloat_Ptr(varIdx, val);
        }

        /// <summary>Pushes a Vector2 style variable.</summary>
        public static unsafe void PushStyleVar(int varIdx, float x, float y)
        {
            if (UI_PushStyleVarVec2_Ptr == null) return;
            UI_PushStyleVarVec2_Ptr(varIdx, x, y);
        }

        /// <summary>Pops style variables.</summary>
        public static unsafe void PopStyleVar(int count = 1)
        {
            if (UI_PopStyleVar_Ptr == null) return;
            UI_PopStyleVar_Ptr(count);
        }

        /// <summary>Inserts empty layout space.</summary>
        public static unsafe void Dummy(float width, float height)
        {
            if (UI_Dummy_Ptr == null) return;
            UI_Dummy_Ptr(width, height);
        }

        /// <summary>Returns true if the last rendered item is hovered.</summary>
        public static unsafe bool IsItemHovered()
        {
            if (UI_IsItemHovered_Ptr == null) return false;
            return UI_IsItemHovered_Ptr() != 0;
        }

        /// <summary>Sets font scale for subsequent text.</summary>
        public static unsafe void SetWindowFontScale(float scale)
        {
            if (UI_SetWindowFontScale_Ptr == null) return;
            UI_SetWindowFontScale_Ptr(scale);
        }

        /// <summary>Renders a float slider. Returns true if modified.</summary>
        public static unsafe bool SliderFloat(string label, ref float value, float min, float max)
        {
            if (label == null || UI_SliderFloat_Ptr == null) return false;
            fixed (char* ptr = label)
            fixed (float* vPtr = &value)
            {
                return UI_SliderFloat_Ptr(ptr, vPtr, min, max) != 0;
            }
        }

        /// <summary>Renders a checkbox. Returns true if clicked/modified.</summary>
        public static unsafe bool Checkbox(string label, ref bool isChecked)
        {
            if (label == null || UI_Checkbox_Ptr == null) return false;
            byte val = (byte)(isChecked ? 1 : 0);
            byte changed = 0;
            fixed (char* ptr = label)
            {
                changed = UI_Checkbox_Ptr(ptr, &val);
            }
            isChecked = (val != 0);
            return changed != 0;
        }
    }

    /// <summary>Standard ImGui color indices.</summary>
    public static class UICol
    {
        public const int Text = 0;
        public const int TextDisabled = 1;
        public const int WindowBg = 2;
        public const int ChildBg = 3;
        public const int PopupBg = 4;
        public const int Border = 5;
        public const int BorderShadow = 6;
        public const int FrameBg = 7;
        public const int FrameBgHovered = 8;
        public const int FrameBgActive = 9;
        public const int Button = 21;
        public const int ButtonHovered = 22;
        public const int ButtonActive = 23;
        public const int Header = 24;
        public const int HeaderHovered = 25;
        public const int HeaderActive = 26;
        public const int Separator = 27;
    }

    /// <summary>Standard ImGui style variable indices.</summary>
    public static class UIStyleVar
    {
        public const int WindowPadding = 2;
        public const int WindowRounding = 3;
        public const int WindowBorderSize = 4;
        public const int ChildRounding = 7;
        public const int ChildBorderSize = 8;
        public const int PopupRounding = 9;
        public const int FramePadding = 11;
        public const int FrameRounding = 12;
        public const int FrameBorderSize = 13;
        public const int ItemSpacing = 14;
        public const int ItemInnerSpacing = 15;
    }
}