namespace ImeIndicator;

internal readonly record struct IndicatorAppearance(Color BackColor, string Text)
{
    public static IndicatorAppearance For(ImeState state) => state switch
    {
        ImeState.Korean => new(IndicatorSettings.KoreanBackColor, IndicatorSettings.KoreanText),
        ImeState.English => new(IndicatorSettings.EnglishBackColor, IndicatorSettings.EnglishText),
        _ => throw new ArgumentOutOfRangeException(nameof(state), state, null),
    };
}
