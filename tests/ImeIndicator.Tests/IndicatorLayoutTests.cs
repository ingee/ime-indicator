using System.Drawing;

namespace ImeIndicator.Tests;

public class IndicatorLayoutTests
{
    [Fact]
    public void GetPosition_PrimaryMonitorAtOrigin_CentersHorizontallyWithTopMargin()
    {
        var monitorBounds = new Rectangle(0, 0, 1920, 1080);

        var position = IndicatorLayout.GetPosition(monitorBounds, indicatorSize: 36, topMargin: 5);

        Assert.Equal(new Point(942, 5), position);
    }

    [Fact]
    public void GetPosition_SecondaryMonitorToTheRight_OffsetsByMonitorOrigin()
    {
        var monitorBounds = new Rectangle(1920, 0, 2560, 1440);

        var position = IndicatorLayout.GetPosition(monitorBounds, indicatorSize: 36, topMargin: 5);

        Assert.Equal(new Point(3182, 5), position);
    }
}
