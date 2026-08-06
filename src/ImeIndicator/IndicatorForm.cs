using System.Drawing.Drawing2D;
using System.Drawing.Text;

namespace ImeIndicator;

internal sealed class IndicatorForm : Form
{
    private const int WS_EX_TOOLWINDOW = 0x00000080;
    private const int WS_EX_NOACTIVATE = 0x08000000;

    private readonly Font _font;
    private string _text = string.Empty;

    // 델리게이트를 필드로 들고 있지 않으면 GC가 수거해서 네이티브 콜백이 끊길 수 있다.
    private readonly WinEventProc _foregroundChangedCallback;
    private IntPtr _foregroundHook;

    public IndicatorForm()
    {
        FormBorderStyle = FormBorderStyle.None;
        ClientSize = new Size(IndicatorSettings.IndicatorSizePx, IndicatorSettings.IndicatorSizePx);
        StartPosition = FormStartPosition.CenterScreen;
        TopMost = true;
        ShowInTaskbar = false;
        DoubleBuffered = true;

        _font = new Font(
            IndicatorSettings.FontFamilyName,
            IndicatorSettings.FontSizePt,
            IndicatorSettings.FontBold ? FontStyle.Bold : FontStyle.Regular);

        _foregroundChangedCallback = OnForegroundWindowChanged;

        // 아직 TSF 연동 전이라 렌더링 확인용으로만 고정값을 적용한다. 이후 항목에서 실제 상태로 교체.
        ApplyAppearance(IndicatorAppearance.For(ImeState.English));
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
