namespace ImeIndicator;

internal static class IndicatorSettings
{
    public const int IndicatorSizePx = 36;
    public const int TopMarginPx = 5;

    public static readonly Color KoreanBackColor = Color.FromArgb(0xFF, 0x00, 0x00); // #FF0000
    public static readonly Color EnglishBackColor = Color.FromArgb(0x00, 0x00, 0xFF); // #0000FF
    public static readonly Color TextColor = Color.White;

    public const string FontFamilyName = "맑은 고딕"; // Malgun Gothic
    public const float FontSizePt = 18f;
    public const bool FontBold = true;

    public const string KoreanText = "한";
    public const string EnglishText = "A";
}
