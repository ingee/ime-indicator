namespace ImeIndicator.Tests;

public class IndicatorStateStoreTests
{
    [Fact]
    public void Toggle_FromEnglish_ChangesCurrentToKorean()
    {
        var store = new IndicatorStateStore(ImeState.English);

        store.Toggle();

        Assert.Equal(ImeState.Korean, store.Current);
    }

    [Fact]
    public void Toggle_FromKorean_ChangesCurrentToEnglish()
    {
        var store = new IndicatorStateStore(ImeState.Korean);

        store.Toggle();

        Assert.Equal(ImeState.English, store.Current);
    }

    [Fact]
    public void Toggle_RaisesChangedWithNewState()
    {
        var store = new IndicatorStateStore(ImeState.English);
        ImeState? notified = null;
        store.Changed += state => notified = state;

        store.Toggle();

        Assert.Equal(ImeState.Korean, notified);
    }

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
