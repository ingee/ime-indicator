namespace ImeIndicator;

// "지금 포커스된 PID → 어떤 상태를 보여줄지" 판단하는 순수 로직. IPC나 OS 훅과 분리해
// 가짜 테이블만으로 TDD할 수 있다.
internal static class ForegroundStateResolver
{
    internal static ImeState Resolve(
        uint currentForegroundPid,
        IReadOnlyDictionary<uint, bool> pidStateTable,
        ImeState lastKnownState)
    {
        if (pidStateTable.TryGetValue(currentForegroundPid, out bool isKoreanOpen))
        {
            return isKoreanOpen ? ImeState.Korean : ImeState.English;
        }

        // 이 PID의 보고가 아직 없음(텍스트 입력을 받지 않는 창 등) — CLAUDE.md 4절:
        // 화면을 건드리지 않고 마지막으로 알려진 상태를 유지한다.
        return lastKnownState;
    }
}
