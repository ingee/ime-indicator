Type: task
Status: resolved

## Question

mintty에서 한/영 전환이 인디케이터에 반영되지 않는 원인은 무엇이며, 어떻게 고치나?

### 배경

- mintty는 Cygwin/MSYS 계열 커스텀 창 — 표준 `GetMessage`/`DispatchMessage` 루프와 다르게
  동작할 수 있다는 게 가설(아직 미검증).
- 인젝션(`ImeFocusHookDll.dll` 로드) 자체가 되는지도 아직 미확인.

### 조사할 것

1. issue 01/02에서 추가한 진단 로그로 재현 — DLL 로드 여부부터 시작.
2. 로드는 되는데 `WM_INIT_TSF`가 안 온다면, mintty의 메시지 루프 구현(mintty/putty 오픈소스
   기준)을 확인해 표준 `DispatchMessage` 경로를 실제로 타는지 조사.

### 결론에 포함할 것

원인 + 수정 여부(구조적으로 대응 불가면 그 판단 근거도 기록 — "알려진 한계"로 문서화할 후보).

## Resolution (2026-08-15)

별도 수정 없이 정상 동작. 이번 세션에서 두 차례 확인됨:
1. 진단 로그(`%TEMP%\ImeFocusHookDll.diag.log`, `%TEMP%\ImeIndicator.diag.log`)로 mintty
   (pid=15300)의 `DoTsfInit` → `AdviseSink` → `CompartmentSink::OnChange` → IPC 보고 →
   `ImeStateIpcListener.Apply` resolved 반영까지 전 구간이 반복적으로 정확히 동작함을 확인.
2. 사용자가 cmd.exe → mintty로 포커스 이동 후 한/영 전환을 두 번 직접 확인 — 인디케이터
   정상 반응.

mintty가 표준 `GetMessage`/`DispatchMessage` 루프와 다르게 동작해 트램폴린 창의
`WM_INIT_TSF`가 씹힐 거라는 가설은 기각됐다 — 실측상 정상 도달·처리됨. cmd.exe/conhost.exe
(issue 01)와 달리 mintty는 GUI 창 소유 프로세스와 TSF Compartment 소유 프로세스가 같아서
(conhost 같은 별도 프로세스를 거치지 않음) PID 불일치 문제 자체가 발생하지 않은 것으로 보임.

**애초에 "무반응"으로 처음 보고됐던 이유(추정)**: 사용자가 이 issue를 처음 발견한 세션 동안
쓰던 mintty 창은, `GET_MODULE_HANDLE_EX_FLAG_PIN`(PR #2, `FocusHookDll.cpp` DllMain) 때문에
그 프로세스가 살아있는 한 `ImeFocusHookDll.dll` 재빌드가 파일 교체 자체에 실패하는 상태였다
(그 mintty 창이 세션의 Bash 도구를 지탱하고 있어 강제 종료도 못 함 — CLAUDE_status.md
2026-08-15 세션 기록). 즉 그 mintty는 여러 차례의 재빌드에도 불구하고 훨씬 오래된 버전의
DLL을 계속 물고 있었을 가능성이 높다. 그 오래된 DLL이 구체적으로 어떤 버그를 안고 있었는지는
그 프로세스가 이미 종료돼 로그가 없어 확인 불가.

이번 세션에서 검증에 쓰인 mintty(pid=15300)는 그 이전 mintty와는 다른 프로세스다 — 어제
세션 마지막 지침에 따라 새로 켠 것으로, `Get-Process mintty`로 확인한 시작 시각이
2026-08-15 19:22:44(오늘 세션 직전)이며, 로그에도 `ResolveReportPid`(어제 세션에 추가된
로직) 호출 흔적이 남아 있어 최소한 어제 마지막 빌드 이후 버전의 DLL을 인젝션받았음이
확인된다. 정상 동작은 이 새 DLL 버전 때문이 아니라(위 문단대로 PID 불일치 문제 자체가
mintty 구조상 없으므로) 오래된 버전에서도 마찬가지였을 것으로 추정 — 다만 이는 재현
불가능한 추정이며 확정할 방법은 없다.
