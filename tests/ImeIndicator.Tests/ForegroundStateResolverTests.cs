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

    // cmd.exe(22060, 눈에 보이는 foreground)와 conhost.exe(16844, TSF Compartment의 실제
    // 소유자이자 22060의 자식) 관계 재현 — .scratch/focus-hook-app-coverage/issues/01-cmd-conhost.md
    [Fact]
    public void Resolve_PidNotInTable_ChildOfForegroundReportedInTable_ReturnsChildState()
    {
        var table = new Dictionary<uint, bool> { [16844] = true };
        var parentPids = new Dictionary<uint, uint> { [16844] = 22060 };

        var result = ForegroundStateResolver.Resolve(22060, table, ImeState.English, parentPids);

        Assert.Equal(ImeState.Korean, result);
    }

    [Fact]
    public void Resolve_PidNotInTable_NoChildMatch_ReturnsLastKnownState()
    {
        var table = new Dictionary<uint, bool> { [9999] = true };
        var parentPids = new Dictionary<uint, uint> { [9999] = 1111 };

        var result = ForegroundStateResolver.Resolve(22060, table, ImeState.English, parentPids);

        Assert.Equal(ImeState.English, result);
    }

    [Fact]
    public void Resolve_DirectMatch_TakesPrecedenceOverParentFallback()
    {
        var table = new Dictionary<uint, bool> { [22060] = false, [16844] = true };
        var parentPids = new Dictionary<uint, uint> { [16844] = 22060 };

        var result = ForegroundStateResolver.Resolve(22060, table, ImeState.Korean, parentPids);

        Assert.Equal(ImeState.English, result);
    }

    [Fact]
    public void Resolve_NullParentPids_NoFallback_ReturnsLastKnownState()
    {
        var table = new Dictionary<uint, bool> { [16844] = true };

        var result = ForegroundStateResolver.Resolve(22060, table, ImeState.English);

        Assert.Equal(ImeState.English, result);
    }
}
