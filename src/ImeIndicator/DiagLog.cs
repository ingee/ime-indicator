namespace ImeIndicator;

// 진단용 임시 로그 — .scratch/focus-hook-app-coverage/issues/01-cmd-conhost.md. 네이티브 쪽
// FocusHookDll.cpp의 LogDiag와 짝을 이뤄, UI가 실제로 어떤 PID를 "현재 포커스"로 보는지와
// IPC로 어떤 PID가 보고되는지를 같이 남겨 PID 불일치 여부를 확인한다. 원인이 밝혀지면 제거한다.
internal static class DiagLog
{
    private static readonly string LogPath = Path.Combine(Path.GetTempPath(), "ImeIndicator.diag.log");
    private static readonly object Lock = new();

    internal static void Log(string message)
    {
        lock (Lock)
        {
            try
            {
                File.AppendAllText(LogPath, $"{DateTime.Now:yyyy-MM-dd HH:mm:ss.fff} {message}{Environment.NewLine}");
            }
            catch (IOException)
            {
            }
        }
    }
}
