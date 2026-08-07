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

        // 프로그램 시작 시 1회만 모니터 구성을 감지한다. 실행 중 모니터 추가/제거는 범위 밖.
        foreach (var screen in Screen.AllScreens)
        {
            int dpi = NativeMethods.GetDpiForBounds(screen.Bounds);
            var form = new IndicatorForm(screen.Bounds, dpi);
            forms.Add(form);
            form.Show();
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
