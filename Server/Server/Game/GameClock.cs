using System;
using System.Diagnostics;

namespace Server.Game
{
    public static class GameClock
    {
        [ThreadStatic]
        private static long _offset;

        public static long Now => Stopwatch.GetTimestamp() - _offset;
        public static double Seconds => Now / (double)Stopwatch.Frequency;

        public static void With(long offset, Action action)
        {
            long previous = _offset;
            _offset = offset;
            try
            {
                action();
            }
            finally
            {
                _offset = previous;
            }
        }

        public static long CurrentOffset => _offset;

        public static long Swap(long offset)
        {
            long previous = _offset;
            _offset = offset;

            return previous;
        }

        public static void Restore(long previous) => _offset = previous;
    }
}
