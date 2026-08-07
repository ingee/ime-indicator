using System.Drawing.Drawing2D;
using System.Drawing.Text;

namespace ImeIndicator;

internal sealed class IndicatorForm : Form
{
    private const int WS_EX_TOOLWINDOW = 0x00000080;
    private const int WS_EX_NOACTIVATE = 0x08000000;

    private readonly Font _font;
    private readonly Rectangle _monitorBounds;
    private string _text = string.Empty;

    // 델리게이트를 필드로 들고 있지 않으면 GC가 수거해서 네이티브 콜백이 끊길 수 있다.
    private readonly WinEventProc _foregroundChangedCallback;
    private IntPtr _foregroundHook;

    // monitorBounds: 이 인디케이터가 속한 모니터의 화면 좌표. initialDpi: 그 모니터의 최초 DPI
    // (IndicatorSettings의 크기값은 96 DPI/100% 배율 기준이라 여기서 실제 DPI로 스케일한다).
    public IndicatorForm(Rectangle monitorBounds, int initialDpi)
    {
        _monitorBounds = monitorBounds;

        AutoScaleMode = AutoScaleMode.None;
        FormBorderStyle = FormBorderStyle.None;
        StartPosition = FormStartPosition.Manual;
        TopMost = true;
        ShowInTaskbar = false;
        DoubleBuffered = true;

        _font = new Font(
            IndicatorSettings.FontFamilyName,
            IndicatorSettings.FontSizePt,
            IndicatorSettings.FontBold ? FontStyle.Bold : FontStyle.Regular);

        _foregroundChangedCallback = OnForegroundWindowChanged;

        ApplyDpiScaledLayout(initialDpi);

        // 아직 TSF 연동 전이라 렌더링 확인용으로만 고정값을 적용한다. 이후 항목에서 실제 상태로 교체.
        ApplyAppearance(IndicatorAppearance.For(ImeState.English));
    }

    // 크기/여백을 IndicatorSettings(96 DPI 기준)에서 주어진 DPI로 다시 스케일해 적용한다.
    private void ApplyDpiScaledLayout(int dpi)
    {
        int sizePx = DpiScaling.Scale(IndicatorSettings.IndicatorSizePx, dpi);
        int topMarginPx = DpiScaling.Scale(IndicatorSettings.TopMarginPx, dpi);

        ClientSize = new Size(sizePx, sizePx);
        Location = IndicatorLayout.GetPosition(_monitorBounds, sizePx, topMarginPx);
    }

    // 창 생성 중 발생하는 WM_DPICHANGED 연쇄는 값이 불안정하게 널뛰는 것이 실측으로 확인됐다
    // (예: 정사각형이 32x47처럼 깨짐). 그 이벤트에 반응하는 대신, 앱 시작이 완전히 끝난 뒤
    // Program.cs가 이 메서드를 한 번 호출해 우리가 직접 조회한 신뢰할 수 있는 DPI로 재보정한다.
    internal void RefreshLayoutForCurrentMonitor()
    {
        int dpi = NativeMethods.GetDpiForBounds(_monitorBounds);
        ApplyDpiScaledLayout(dpi);

        // 크기가 이전과 같으면 WinForms가 리사이즈로 보지 않아 다시 그리지 않을 수 있다.
        // 최초 페인트가 DPI 컨텍스트가 아직 불안정하던 시점에 일어났을 수 있으므로 항상
        // 다시 그리게 강제한다.
        Invalidate();
    }

    // Win+Shift+S 캡처 오버레이 같은 시스템 UI가 뜨면 다른 앱의 topmost 상태가 풀리는 경우가
    // 있다(Windows 자체 동작). 포그라운드 창이 바뀔 때마다 이벤트로 통지받아 topmost를 다시
    // 걸어준다 — 주기적으로 재확인하는 타이머 폴링이 아니라 OS 이벤트 구독이다.
    protected override void OnHandleCreated(EventArgs e)
    {
        base.OnHandleCreated(e);

        _foregroundHook = NativeMethods.SetWinEventHook(
            NativeMethods.EVENT_SYSTEM_FOREGROUND,
            NativeMethods.EVENT_SYSTEM_FOREGROUND,
            IntPtr.Zero,
            _foregroundChangedCallback,
            0,
            0,
            NativeMethods.WINEVENT_OUTOFCONTEXT | NativeMethods.WINEVENT_SKIPOWNPROCESS);
    }

    protected override void OnHandleDestroyed(EventArgs e)
    {
        if (_foregroundHook != IntPtr.Zero)
        {
            NativeMethods.UnhookWinEvent(_foregroundHook);
            _foregroundHook = IntPtr.Zero;
        }

        base.OnHandleDestroyed(e);
    }

    private void OnForegroundWindowChanged(
        IntPtr hWinEventHook, uint eventType, IntPtr hwnd, int idObject, int idChild, uint dwEventThread, uint dwmsEventTime)
    {
        if (!IsHandleCreated || IsDisposed)
        {
            return;
        }

        NativeMethods.SetWindowPos(
            Handle,
            NativeMethods.HWND_TOPMOST,
            0, 0, 0, 0,
            NativeMethods.SWP_NOMOVE | NativeMethods.SWP_NOSIZE | NativeMethods.SWP_NOACTIVATE);
    }

    protected override CreateParams CreateParams
    {
        get
        {
            var createParams = base.CreateParams;
            createParams.ExStyle |= WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
            return createParams;
        }
    }

    internal void ApplyAppearance(IndicatorAppearance appearance)
    {
        BackColor = appearance.BackColor;
        _text = appearance.Text;
        Invalidate();
    }

    // Label.TextAlign 등 GDI의 기본 중앙 정렬은 폰트의 줄 높이(ascent+descent) 기준이라,
    // "한"처럼 위아래로 꽉 찬 글자와 "A"처럼 descender가 없는 짧은 글자가 같은 폰트 안에서
    // 서로 다른 위치에 놓여 보인다. 실제 잉크 영역(ink bounds)을 측정해 그 영역 자체를
    // 클라이언트 사각형 중앙에 맞춘다.
    protected override void OnPaint(PaintEventArgs e)
    {
        base.OnPaint(e);

        if (string.IsNullOrEmpty(_text))
        {
            return;
        }

        var graphics = e.Graphics;
        graphics.SmoothingMode = SmoothingMode.AntiAlias;
        graphics.TextRenderingHint = TextRenderingHint.AntiAliasGridFit;

        float emSizeInPixels = _font.SizeInPoints * graphics.DpiY / 72f;

        using var path = new GraphicsPath();
        path.AddString(_text, _font.FontFamily, (int)_font.Style, emSizeInPixels, PointF.Empty, StringFormat.GenericTypographic);

        var inkBounds = path.GetBounds();
        float dx = ((ClientSize.Width - inkBounds.Width) / 2f) - inkBounds.X;
        float dy = ((ClientSize.Height - inkBounds.Height) / 2f) - inkBounds.Y;

        using var offset = new Matrix();
        offset.Translate(dx, dy);
        path.Transform(offset);

        using var brush = new SolidBrush(IndicatorSettings.TextColor);
        graphics.FillPath(brush, path);
    }
}
