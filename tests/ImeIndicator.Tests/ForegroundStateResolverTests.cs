namespace ImeIndicator.Tests;

public class ForegroundStateResolverTests
{
    [Fact]
    public void Resolve_PidInTableTrue_ReturnsKorean()
    {
        var table = new Dictionary<uint, bool> { [1234] = true };

        var result = ForegroundStateResolver.Resolve(1234, table, ImeState.English);

        Assert.Equal(ImeState.Korean, result);
    }

    [Fact]
    public void Resolve_PidInTableFalse_ReturnsEnglish()
    {
        var table = new Dictionary<uint, bool> { [1234] = false };

        var result = ForegroundStateResolver.Resolve(1234, table, ImeState.Korean);

        Assert.Equal(ImeState.English, result);
    }

    [Fact]
    public void Resolve_PidNotInTable_ReturnsLastKnownState()
    {
        var table = new Dictionary<uint, bool> { [9999] = true };

        var result = ForegroundStateResolver.Resolve(1234, table, ImeState.English);

        Assert.Equal(ImeState.English, result);
    }

    [Fact]
    public void Resolve_PidNotInTable_WithLastKnownKorean_ReturnsKorean()
    {
        var table = new Dictionary<uint, bool> { [9999] = false };

        var result = ForegroundStateResolver.Resolve(1234, table, ImeState.Korean);

        Assert.Equal(ImeState.Korean, result);
    }

    [Fact]
    public void Resolve_EmptyTable_ReturnsLastKnownState()
    {
        var table = new Dictionary<uint, bool>();

        var result = ForegroundStateResolver.Resolve(1234, table, ImeState.Korean);

        Assert.Equal(ImeState.Korean, result);
    }
}
