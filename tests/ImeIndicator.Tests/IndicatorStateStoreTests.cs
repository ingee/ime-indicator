namespace ImeIndicator.Tests;

public class IndicatorStateStoreTests
{
    [Fact]
    public void Set_ToDifferentState_UpdatesCurrentAndRaisesChanged()
    {
        var store = new IndicatorStateStore(ImeState.English);
        ImeState? notified = null;
        store.Changed += state => notified = state;

        store.Set(ImeState.Korean);

        Assert.Equal(ImeState.Korean, store.Current);
        Assert.Equal(ImeState.Korean, notified);
    }
}
