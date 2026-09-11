namespace Chained
{
    /// <summary>Clipboard helper for copying text to the OS clipboard via GLFW.</summary>
    public static class Clipboard
    {
#pragma warning disable 0649
        internal static unsafe delegate* unmanaged<char*, void> Clipboard_SetText_Ptr;
#pragma warning restore 0649

        /// <summary>Copies the given text to the OS clipboard.</summary>
        public static unsafe void SetText(string text)
        {
            if (string.IsNullOrEmpty(text)) return;
            if (Clipboard_SetText_Ptr != null)
            {
                fixed (char* ptr = text)
                {
                    Clipboard_SetText_Ptr(ptr);
                }
            }
        }
    }
}
