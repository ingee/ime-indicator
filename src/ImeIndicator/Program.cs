namespace ImeIndicator;

static class Program
{
    /// <summary>
    ///  The main entry point for the application.
    /// </summary>
    [STAThread]
    static void Main()
    {
        // To customize application configuration such as set high DPI settings or default font,
        // see https://aka.ms/applicationconfiguration.
        ApplicationConfiguration.Initialize();

        var forms = new List<IndicatorForm>();

        // 모든 모니터의 인디케이터가 공유하는 단일 상태. 마지막으로 알려진 상태가 없는
        // 시작 시점이므로 영문(파랑/"A")을 기본값으로 시작한다.
        var stateStore = new IndicatorStateStore(ImeState.English);

        // 프로그램 시작 시 1회만 모니터 구성을 감지한다. 실행 중 모니터 추가/제거는 범위 밖.
        foreach (var screen in Screen.AllScreens)
        {
            int dpi = NativeMethods.GetDpiForBounds(screen.Bounds);
            var form = new IndicatorForm(screen.Bounds, dpi, stateStore);
            forms.Add(form);
            form.Show();
        }

        // 폼을 최소 하나 이상 만든 뒤라 WindowsFormsSynchronizationContext가 이미 설치돼
        // 있다. IPC 리스너는 백그라운드 스레드에서 파이프 메시지를 받으므로, WinForms 컨트롤을
        // 건드리는 상태 반영은 이 컨텍스트로 마샬링해 UI 스레드에서만 실행되게 한다.
        var uiContext = SynchronizationContext.Current;
        var foregroundTracker = new ForegroundWindowTracker();

        // TIP DLL이 아직 등록되지 않았거나 파이프 서버를 못 띄워도(방어적 처리) 인디케이터는
        // 기본값(영문)으로 계속 떠 있는다. 상태 갱신은 오직 IPC 메시지 + 포커스 전환 이벤트로만
        // 트리거된다 — 폴링 없음.
        try
        {
            new ImeStateIpcListener(stateStore, foregroundTracker, uiContext).Start();
        }
        catch (IOException)
        {
        }

        // 창 생성 도중의 DPI 협상 과정(WM_DPICHANGED 연쇄)이 불안정한 것으로 확인되어,
        // 앱이 완전히 시작을 마친 뒤(Idle) 각 창의 크기/위치를 한 번 더 확실하게 재보정한다.
        void OnIdleOnce(object? sender, EventArgs e)
        {
            Application.Idle -= OnIdleOnce;
            foreach (var form in forms)
            {
                form.RefreshLayoutForCurrentMonitor();
            }
        }

        Application.Idle += OnIdleOnce;

        Application.Run();
    }
}
