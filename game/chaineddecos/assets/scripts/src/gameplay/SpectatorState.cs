using System;
using Chained;

namespace ChainedDecos.Scripts
{
    public static class SpectatorState
    {
        public static bool IsFinished { get; set; } = false;
        public static float FinishTime { get; set; } = 0.0f;

        public static void Reset()
        {
            IsFinished = false;
            FinishTime = 0.0f;
        }

        public static void TriggerFinish(float elapsedTime)
        {
            if (IsFinished) return;
            IsFinished = true;
            FinishTime = elapsedTime;
            Log.Info($"[SpectatorState] Level completed! Finish time: {FormatTime(elapsedTime)}");

            if (Network.IsConnected)
            {
                string localName = Network.GetLocalPlayerName();
                if (string.IsNullOrWhiteSpace(localName)) localName = "Player";
                Network.SendChatMessage($"[VICTORY] {localName} has reached the finish line in {FormatTime(elapsedTime)}!");
            }
        }

        public static string FormatTime(float totalSeconds)
        {
            int hours = (int)(totalSeconds / 3600.0f);
            int minutes = (int)((totalSeconds - hours * 3600.0f) / 60.0f);
            int seconds = (int)(totalSeconds) % 60;
            int hundredths = (int)((totalSeconds - MathF.Floor(totalSeconds)) * 100.0f);

            if (hours > 0)
                return $"{hours:D2}:{minutes:D2}:{seconds:D2}.{hundredths:D2}";
            return $"{minutes:D2}:{seconds:D2}.{hundredths:D2}";
        }
    }
}
