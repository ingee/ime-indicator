Type: task
Status: resolved

## Question

cmd.exe(conhost.exe)에서 한/영 전환이 인디케이터에 반영되지 않는 원인은 무엇이며, 어떻게 고치나?

### 배경

- 지난 세션(2026-08-15)에 `ImeFocusHookDll.dll`이 `conhost.exe`(cmd.exe의 실제 창 소유자)에
  정상적으로 로드됨을 모듈 목록으로 직접 확인함 — 인젝션 자체는 성공. 4개 이슈 중 유일하게
  "인젝션 성공"이 이미 확인된 케이스.
- `.scratch/ime-detection-strategy/issues/06-cmd-onsetfocus-and-excel-reverification.md`에
  구 TIP 시절 "PID 스왑" 의심이 이미 기록돼 있음 — 새 아키텍처(ADR-0007)에서도 재현되는지
  아직 실측 안 됨. `EVENT_SYSTEM_FOREGROUND`의 `hwnd` 소유 PID가 `cmd.exe`로 보고되는
  상황에서 실제 인젝션이 `cmd.exe`/`conhost.exe` 중 어디로 되는지 확인 필요
  (`.scratch/ime-detection-strategy/map.md` "Not yet specified" 항목 계승).
- `src/ImeFocusHook/FocusHookDll.cpp`의 `TrampolineWndProc`(177행 근처)과 `DoTsfInit()`
  (108행 근처)에 아직 진단 로그가 없음 — 어느 단계에서 끊기는지 불명.

### 조사할 것

1. `DoTsfInit()`/`TrampolineWndProc`에 단계별 임시 로그 추가(파일 또는
   `OutputDebugString`) — 트램폴린 창 생성 여부, `WM_INIT_TSF` 수신 여부, TSF 각 단계
   (CoInitialize/CoCreateInstance/Activate/QI/GetCompartment/AdviseSink) 성공 여부를
   구분해서 남긴다. 이 로그는 issue 02(새 메모장)·03(mintty)에도 그대로 재사용한다.
2. `scripts/build-release.ps1`로 재빌드 후 cmd.exe에서 재현, 로그로 정확히 어느 단계에서
   끊기는지 확인.
3. PID 스왑 여부(위 배경 참고) — `FocusHookProc`에서 관측한 `windowPid`와 실제 인젝션된
   프로세스의 PID(`GetCurrentProcessId()`)가 일치하는지 로그로 같이 남긴다.
4. "메시지가 아예 안 옴"으로 확인되면, 메시지 루프에 의존하지 않는 마샬링(예: 워커 스레드 +
   `QueueUserAPC`) 검토 — 이 경우 트램폴린 창 구조 자체를 다시 손봐야 할 수 있음.

### 결론에 포함할 것

원인(메시지 미도달 / PID 불일치 / TSF 단계 실패 등) + 적용한 수정 + 재검증 결과.

## Resolution (2026-08-15)

**원인**: PID 불일치. `ImeFocusHookDll.dll`은 `conhost.exe`(cmd.exe의 실제 창 소유자,
TSF Compartment 진짜 소유자)에 정상 인젝션되고 TSF 구독·`OnChange`도 100% 정확히 동작했다
(DLL/TSF 레벨은 처음부터 문제 없었음). 문제는 PID 해석 단계였다: UI(`ImeIndicator.exe`,
별도 프로세스)에서 `GetForegroundWindow()`를 부르면 cmd.exe(예: PID 22060)를 정확히
가리키는데, **같은 API를 conhost.exe 프로세스 내부에서 호출하면 시스템 전역 관측값과 다르게
자기 자신(conhost, 예: PID 16844)을 가리킨다** — 왜 프로세스마다 다른 값이 나오는지
근본 원인(별도 데스크톱/윈도우 스테이션 여부 등)은 끝내 못 밝혔지만, 실측으로 재현성은
확인됐고 우회로 실용적으로 해결했다.

**적용한 수정**: `GetForegroundWindow()` 자체를 더 파고드는 대신, 프로세스 트리 관계를
활용하는 폴백을 추가했다. `Get-CimInstance Win32_Process`로 conhost.exe의
`ParentProcessId`가 정확히 cmd.exe의 PID임을 확인 — UI가 관측한 foreground PID(cmd.exe)로
테이블 직접 매치가 실패하면, 테이블에 보고된 PID 중 그 foreground PID를 부모로 둔 것을
대신 찾는다.
- `src/ImeIndicator/ForegroundStateResolver.cs`: `Resolve()`에 `parentPids` 옵션 파라미터
  추가, 직접 매치 실패 시 부모-자식 매칭 폴백.
- `src/ImeIndicator/ProcessTree.cs`(신규): `CreateToolhelp32Snapshot`으로 PID→부모PID
  스냅샷을 찍는 `SnapshotParentPids()`. WMI(`Get-CimInstance`)보다 가벼움.
- `src/ImeIndicator/NativeMethods.cs`: `PROCESSENTRY32` 구조체 + Toolhelp32Snapshot
  P/Invoke 3종 추가.
- `src/ImeIndicator/ImeStateIpcListener.cs`: `Apply()`에서 직접 매치 실패(`hasEntry=false`)
  시에만 `ProcessTree.SnapshotParentPids()` 호출 — 일반 앱(직접 매치 성공)에는 오버헤드 없음.
- 단위 테스트 4개 추가(`tests/ImeIndicator.Tests/ForegroundStateResolverTests.cs`).

**재검증 결과**: release 재빌드 후 cmd.exe를 새로 열고 한/영 전환 → 인디케이터 정상 반응
확인(사용자 직접 확인). 진단 로그로도 폴백 경로 작동을 확인:
`foregroundPid=20332 hasEntry=False ... resolved=Korean` — 테이블에 20332(cmd.exe) 직접
매치는 없지만, 그 자식인 16916(conhost.exe)의 보고 상태를 정확히 반영했다.

**진단 로그 정리는 다음 세션 과제로 남김** — `LogDiag`(FocusHookDll.cpp)와 `DiagLog.cs` 및
호출부는 issue 02/03에서도 재사용 예정이라 이번엔 유지.
