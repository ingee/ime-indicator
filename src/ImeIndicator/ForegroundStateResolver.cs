namespace ImeIndicator;

// "지금 포커스된 PID → 어떤 상태를 보여줄지" 판단하는 순수 로직. IPC나 OS 훅과 분리해
// 가짜 테이블만으로 TDD할 수 있다.
internal static class ForegroundStateResolver
{
    internal static ImeState Resolve(
        uint currentForegroundPid,
        IReadOnlyDictionary<uint, bool> pidStateTable,
        ImeState lastKnownState,
        IReadOnlyDictionary<uint, uint>? parentPids = null)
    {
        if (pidStateTable.TryGetValue(currentForegroundPid, out bool isKoreanOpen))
        {
            return isKoreanOpen ? ImeState.Korean : ImeState.English;
        }

        // cmd.exe처럼, TSF Compartment를 실제로 보고하는 프로세스(conhost.exe)가 눈에 보이는
        // foreground 프로세스의 자식인 경우가 있다(.scratch/focus-hook-app-coverage/issues/
        // 01-cmd-conhost.md 실측). 직접 매치가 안 되면 테이블에 보고된 PID 중 foreground PID를
        // 부모로 둔 것을 대신 찾는다.
        if (parentPids is not null)
        {
            foreach (var (pid, isKoreanOpenEntry) in pidStateTable)
            {
                if (parentPids.TryGetValue(pid, out uint parentPid) && parentPid == currentForegroundPid)
                {
                    return isKoreanOpenEntry ? ImeState.Korean : ImeState.English;
                }
            }
        }

        // 이 PID의 보고가 아직 없음(텍스트 입력을 받지 않는 창 등) — CLAUDE.md 4절:
        // 화면을 건드리지 않고 마지막으로 알려진 상태를 유지한다.
        return lastKnownState;
    }
}
