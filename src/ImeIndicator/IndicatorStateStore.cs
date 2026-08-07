namespace ImeIndicator;

internal sealed class IndicatorStateStore
{
    public ImeState Current { get; private set; }

    public event Action<ImeState>? Changed;

    public IndicatorStateStore(ImeState initial)
    {
        Current = initial;
    }

    public void Toggle()
    {
        Current = Current == ImeState.Korean ? ImeState.English : ImeState.Korean;
        Changed?.Invoke(Current);
    }

    public void Set(ImeState newState)
    {
        Current = newState;
        Changed?.Invoke(Current);
    }
}
