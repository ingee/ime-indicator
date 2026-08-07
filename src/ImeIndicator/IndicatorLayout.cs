namespace ImeIndicator;

internal static class IndicatorLayout
{
    public static Point GetPosition(Rectangle monitorBounds, int indicatorSize, int topMargin)
    {
        int x = monitorBounds.Left + ((monitorBounds.Width - indicatorSize) / 2);
        int y = monitorBounds.Top + topMargin;
        return new Point(x, y);
    }
}
