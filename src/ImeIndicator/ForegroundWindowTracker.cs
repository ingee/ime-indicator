namespace ImeIndicator;

// 지금 OS 포커스를 가진 프로세스의 PID를 추적하는 단일 컴포넌트(Program.cs에서 한 번만
// 생성 — 모니터별 IndicatorForm과는 별개다). SetWinEventHook(EVENT_SYSTEM_FOREGROUND)는
// TSF와 무관한 표준 Win32 API라 폴링 없이 이벤트로만 갱신된다. IndicatorForm.cs가 이미
// 같은 훅을 topmost 재적용 용도로 쓰고 있지만, 그건 별개 관심사라 건드리지 않는다 — 한
// 프로세스에 EVENT_SYSTEM_FOREGROUND 훅이 여러 개 공존하는 것은 정상 지원되는 사용법이다.
internal sealed class ForegroundWindowTracker : IDisposable
{
    internal event Action<uint>? ForegroundPidChanged;

    // 콘솔 앱(cmd.exe)은 포커스 전환 시 EVENT_SYSTEM_FOREGROUND가 짧은 간격(수 ms)으로 두 번
    // 발생한다 — 처음엔 cmd.exe 소유 창, 곧이어 conhost.exe(TSF Compartment의 진짜 소유자)
    // 소유의 실제 콘솔 창(.scratch/focus-hook-app-coverage/issues/01-cmd-conhost.md 실측).
    // OUTOFCONTEXT 훅은 이 두 번째 이벤트를 놓치는 경우가 있어(마샬링 경합으로 추정), 값을
    // 훅 콜백에서 캐시해두면 그 유실된 순간의 낡은 PID가 계속 남는다. 그래서 캐시하지 않고
    // 조회할 때마다(Apply() 등 실제 판단이 필요한 시점마다) 매번 새로 조회한다 — 훅 콜백은
    // "지금 다시 판단해야 한다"는 신호로만 쓰고, 그 순간 GetForegroundWindow()는 폴링이 아니라
    // 실제 이벤트(훅 발화 또는 IPC 보고 수신)에 반응해서만 호출된다.
    internal uint CurrentForegroundPid => GetForegroundPid();

    // 델리게이트를 필드로 들고 있지 않으면 GC가 수거해서 네이티브 콜백이 끊길 수 있다.
    private readonly WinEventProc _foregroundChangedCallback;
    private IntPtr _hook;

    internal ForegroundWindowTracker()
    {
        _foregroundChangedCallback = OnForegroundWindowChanged;

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
        DiagLog.Log($"ForegroundWindowTracker: eventHwnd={hwnd} liveForegroundPid={CurrentForegroundPid}");
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
