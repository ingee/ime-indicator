namespace ImeIndicator;

internal static class DpiScaling
{
    private const int BaselineDpi = 96;

    public static int Scale(int valueAt96Dpi, int actualDpi) =>
        (int)Math.Round(valueAt96Dpi * (actualDpi / (double)BaselineDpi));
}
