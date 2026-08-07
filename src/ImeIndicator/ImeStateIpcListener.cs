using System.IO.Pipes;

namespace ImeIndicator;

// TIP DLL들(프로세스마다 하나씩)이 (PID, 한/영 상태)를 보고하는 named pipe 서버 + 그 보고를
// 모아두는 PID→상태 테이블. ForegroundWindowTracker의 포커스 전환 이벤트와 새 IPC 보고,
// 둘 중 어느 쪽이 오든 같은 판단 경로(ForegroundStateResolver.Resolve)를 거쳐
// IndicatorStateStore.Set을 호출한다. 폴링 없이 파이프 연결/이벤트로만 갱신된다.
internal sealed class ImeStateIpcListener : IDisposable
{
    // TIP 쪽 IpcClient.cpp의 kPipeName과 반드시 일치해야 하는 교차 언어 상수(공유 헤더 불가).
    // 시스템에 등록되는 자원 이름은 ingee로 시작한다는 규칙 적용.
    private const string PipeName = "ingee.ImeIndicator.StateReport";
    private const int MessageSize = 5; // uint32 pid + uint8 isKoreanOpen

    private readonly IndicatorStateStore _stateStore;
    private readonly ForegroundWindowTracker _foregroundTracker;
    private readonly SynchronizationContext? _uiContext;
    private readonly Dictionary<uint, bool> _pidStateTable = new();
    private readonly object _tableLock = new();
    private CancellationTokenSource? _cts;

    internal ImeStateIpcListener(IndicatorStateStore stateStore, ForegroundWindowTracker foregroundTracker, SynchronizationContext? uiContext)
    {
        _stateStore = stateStore;
        _foregroundTracker = foregroundTracker;
        _uiContext = uiContext;
        _foregroundTracker.ForegroundPidChanged += _ => Reevaluate();
    }

    internal void Start()
    {
        _cts = new CancellationTokenSource();
        _ = AcceptLoopAsync(_cts.Token);
    }

    private async Task AcceptLoopAsync(CancellationToken token)
    {
        while (!token.IsCancellationRequested)
        {
            try
            {
                using var pipe = new NamedPipeServerStream(
                    PipeName, PipeDirection.In, NamedPipeServerStream.MaxAllowedServerInstances,
                    PipeTransmissionMode.Byte, PipeOptions.Asynchronous);

                await pipe.WaitForConnectionAsync(token).ConfigureAwait(false);

                byte[] buffer = new byte[MessageSize];
                int totalRead = 0;
                while (totalRead < MessageSize)
                {
                    int read = await pipe.ReadAsync(buffer.AsMemory(totalRead, MessageSize - totalRead), token).ConfigureAwait(false);
                    if (read == 0)
                    {
                        break;
                    }
                    totalRead += read;
                }

                if (totalRead == MessageSize)
                {
                    uint pid = BitConverter.ToUInt32(buffer, 0);
                    bool isKoreanOpen = buffer[4] != 0;
                    UpdateTable(pid, isKoreanOpen);
                }
            }
            catch (OperationCanceledException)
            {
                break;
            }
            catch (Exception)
            {
                // 클라이언트가 중간에 끊기는 등 파이프 I/O 오류는 무시하고 계속 리슨한다
                // (CLAUDE.md 4절: 크래시 없이 인디케이터가 유지되어야 함).
                await Task.Delay(50, token).ContinueWith(_ => { }, TaskScheduler.Default).ConfigureAwait(false);
            }
        }
    }

    private void UpdateTable(uint pid, bool isKoreanOpen)
    {
        lock (_tableLock)
        {
            _pidStateTable[pid] = isKoreanOpen;
        }
        Reevaluate();
    }

    // ForegroundWindowTracker.CurrentForegroundPid는 UI 스레드에서만 쓰이므로, 이 메서드도
    // 항상 UI 스레드에서 실행되도록 SynchronizationContext로 마샬링한다.
    private void Reevaluate()
    {
        if (_uiContext is null)
        {
            Apply();
            return;
        }

        _uiContext.Post(_ => Apply(), null);
    }

    private void Apply()
    {
        Dictionary<uint, bool> snapshot;
        lock (_tableLock)
        {
            snapshot = new Dictionary<uint, bool>(_pidStateTable);
        }

        ImeState resolved = ForegroundStateResolver.Resolve(_foregroundTracker.CurrentForegroundPid, snapshot, _stateStore.Current);
        if (resolved != _stateStore.Current)
        {
            _stateStore.Set(resolved);
        }
    }

    public void Dispose()
    {
        _cts?.Cancel();
        _cts?.Dispose();
    }
}
