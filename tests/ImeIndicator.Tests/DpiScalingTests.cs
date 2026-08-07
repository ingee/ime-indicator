namespace ImeIndicator.Tests;

public class DpiScalingTests
{
    [Fact]
    public void Scale_At96Dpi_ReturnsValueUnchanged()
    {
        int scaled = DpiScaling.Scale(valueAt96Dpi: 26, actualDpi: 96);

        Assert.Equal(26, scaled);
    }

    [Fact]
    public void Scale_At144Dpi_Returns150PercentOfValue()
    {
        int scaled = DpiScaling.Scale(valueAt96Dpi: 26, actualDpi: 144);

        Assert.Equal(39, scaled);
    }
}
