namespace ImeIndicator;

// 지금 OS 포커스를 가진 프로세스의 PID를 추적하는 단일 컴포넌트(Program.cs에서 한 번만
// 생성 — 모니터별 IndicatorForm과는 별개다). SetWinEventHook(EVENT_SYSTEM_FOREGROUND)는
// TSF와 무관한 표준 Win32 API라 폴링 없이 이벤트로만 갱신된다. IndicatorForm.cs가 이미
// 같은 훅을 topmost 재적용 용도로 쓰고 있지만, 그건 별개 관심사라 건드리지 않는다 — 한
// 프로세스에 EVENT_SYSTEM_FOREGROUND 훅이 여러 개 공존하는 것은 정상 지원되는 사용법이다.
internal sealed class ForegroundWindowTracker : IDisposable
{
    internal event Action<uint>? ForegroundPidChanged;

    internal uint CurrentForegroundPid { get; private set; }

    // 델리게이트를 필드로 들고 있지 않으면 GC가 수거해서 네이티브 콜백이 끊길 수 있다.
    private readonly WinEventProc _foregroundChangedCallback;
    private IntPtr _hook;

    internal ForegroundWindowTracker()
    {
        _foregroundChangedCallback = OnForegroundWindowChanged;

        // 훅 이벤트는 "바뀔 때"만 오므로, 시작 시 1회는 직접 조회해 초기값을 채운다
        // (CLAUDE.md 4절 — 이후로는 콜백에만 의존).
        CurrentForegroundPid = GetForegroundPid();

        _hook = NativeMethods.SetWinEventHook(
            NativeMethods.EVENT_SYSTEM_FOREGROUND,
            NativeMethods.EVENT_SYSTEM_FOREGROUND,
            IntPtr.Zero,
            _foregroundChangedCallback,
            0,
            0,
            NativeMethods.WINEVENT_OUTOFCONTEXT | NativeMethods.WINEVENT_SKIPOWNPROCESS);
    }

    private void OnForegroundWindowChanged(
        IntPtr hWinEventHook, uint eventType, IntPtr hwnd, int idObject, int idChild, uint dwEventThread, uint dwmsEventTime)
    {
        CurrentForegroundPid = GetForegroundPid();
        ForegroundPidChanged?.Invoke(CurrentForegroundPid);
    }

    private static uint GetForegroundPid()
    {
        IntPtr hwnd = NativeMethods.GetForegroundWindow();
        if (hwnd == IntPtr.Zero)
        {
            return 0;
        }

        NativeMethods.GetWindowThreadProcessId(hwnd, out uint pid);
        return pid;
    }

    public void Dispose()
    {
        if (_hook != IntPtr.Zero)
        {
            NativeMethods.UnhookWinEvent(_hook);
            _hook = IntPtr.Zero;
        }
    }
}
