namespace ImeIndicator.Tests;

public class IndicatorAppearanceTests
{
    [Fact]
    public void For_Korean_ReturnsRedBackgroundAndHanText()
    {
        var appearance = IndicatorAppearance.For(ImeState.Korean);

        Assert.Equal(IndicatorSettings.KoreanBackColor, appearance.BackColor);
        Assert.Equal(IndicatorSettings.KoreanText, appearance.Text);
    }

    [Fact]
    public void For_English_ReturnsBlueBackgroundAndAText()
    {
        var appearance = IndicatorAppearance.For(ImeState.English);

        Assert.Equal(IndicatorSettings.EnglishBackColor, appearance.BackColor);
        Assert.Equal(IndicatorSettings.EnglishText, appearance.Text);
    }
}
