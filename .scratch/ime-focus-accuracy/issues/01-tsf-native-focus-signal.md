# TSF 네이티브 포커스 신호로 UI의 포커스 판단을 대체/보완할 수 있는가

Type: research
Status: resolved

## Question

UI가 "지금 TSF 포커스를 가진 프로세스"를 판단하는 방법을, 지금처럼 독립적인 Win32
`SetWinEventHook(EVENT_SYSTEM_FOREGROUND)` 기반 PID 추적 대신, TIP이 TSF 자체의 포커스 개념
(`ITfThreadMgrEventSink::OnSetFocus` 또는 동급 메커니즘)을 IPC로 함께 보고하는 방식으로 대체하거나
보완할 수 있는가?

### 배경

- 현재 아키텍처(ADR-0005): TIP DLL이 프로세스마다 로드되어 스레드 스코프
  `GUID_COMPARTMENT_KEYBOARD_OPENCLOSE`를 구독해 (PID, 상태)를 IPC로 보고한다. UI는 별도로
  `SetWinEventHook`으로 포그라운드 PID를 추적해, 보고받은 테이블에서 그 PID를 찾아 표시한다.
- 실측 확인된 문제: `cmd.exe`에서 `GetForegroundWindow()`가 반환하는 보이는 창의 PID는 `cmd.exe`
  자신이지만, 실제 TSF/TIP이 로드되고 상태를 보고하는 프로세스는 자식 프로세스인 `conhost.exe`다
  (2026-08-08 실측 재확인 — `cmd.exe`를 강제 포그라운드에 놓고 `GetForegroundWindow` +
  `GetWindowThreadProcessId`를 직접 호출, `ConsoleWindowClass` 창의 소유 PID가 `cmd.exe` 자신임을
  확인).
- 사전 조사(Google Project Zero, Tavis Ormandy, 2019)로 Windows 자체 입력 표시기도 "주입 없이
  원격 관찰"하는 지름길은 없고, 우리와 구조적으로 같은 "프로세스마다 클라이언트 로드 + 중앙 relay"
  패턴을 쓴다는 게 확인됨 — 즉 아키텍처의 큰 틀(주입+push)은 유지하되, "UI가 포커스를 판단하는
  방법"만 바꾸는 방향이 유력하다.

### 조사할 것

1. `ITfThreadMgrEventSink::OnSetFocus`(또는 관련 포커스 알림 인터페이스)가, 보이는 창과 실제 TIP
   로드 프로세스가 다른 `conhost.exe` 같은 경우에도 "그 프로세스/스레드가 지금 실제로 텍스트
   입력을 받고 있다"는 신호로 신뢰성 있게 발생하는지. (1차 자료: MS Learn TSF 문서, Windows SDK
   헤더 주석, 신뢰할 만한 TSF 내부 구조 해설)
2. 이 신호를 채택하면 UI 쪽 `SetWinEventHook`을 완전히 대체할 수 있는지, 아니면 기존 방식과
   병행(예: 포커스 PID가 상태 테이블에 없을 때만 TSF 포커스 신호로 폴백)해야 하는지 — 장단점 비교.
3. ADR-0005가 이미 발견한 "PID는 일치하지만 스레드ID는 최신 패키지형(WinUI) 앱에서 창 소유
   스레드와 TIP 활성화 스레드가 불일치"라는 문제가, 스레드 스코프 포커스 이벤트를 쓸 때도 그대로
   재현되는지 — 즉 이 신호를 프로세스 단위로 집계해서 PID 매칭에 쓸 수 있는지.
4. 폴링 없이 이 신호를 구독하는 게 CLAUDE.md의 "폴링 최대한 회피" 원칙과 부합하는지(이벤트 기반일
   것으로 예상되지만 확인).

### 결론에 포함할 것

채택 여부 권고 + 채택 시 TIP/UI 각각의 책임이 어떻게 바뀌는지 요약. 근거는 1차 자료 인용.

## Answer

전체 조사: `docs/research/tsf-native-focus-signal.md` (브랜치 `research/tsf-native-focus-signal`,
커밋 `f1c88b0`).

**채택 권고: 병행(보완) — `SetWinEventHook` 완전 대체 아님.**

- `ITfThreadMgrEventSink::OnSetFocus`는 TIP이 `Activate()`에서 받는 그 프로세스의 스레드 스코프
  `ITfThreadMgr`에 `ITfSource::AdviseSink`로 그대로 걸 수 있는 폴링 없는 콜백(MS Learn 1차 자료
  확인, 신뢰도 높음).
- `cmd.exe`/`conhost.exe` PID 불일치는 `conhost.exe`가 콘솔 창의 "소유 PID/TID"를 커널 레벨에서
  클라이언트 프로세스로 바꿔치기하는 문서화된 오퍼레이션(`ConsoleSetWindowOwner`) 때문일 가능성이
  높음(메커니즘 존재는 신뢰도 높음, 이 프로젝트 증상과의 인과관계는 추론). TIP은
  `GetCurrentProcessId()`로 자기 프로세스 ID를 직접 얻으므로 이 창-소유권 바꿔치기에 영향받지
  않음 — 이것이 채택 이유.
- 완전 대체를 안 하는 이유: `OnSetFocus`는 TIP이 `Activate()`된 프로세스에서만 존재 가능 — Excel처럼
  `Activate()`조차 안 되는 프로세스(티켓 02)에서는 이 신호 자체가 없음. `SetWinEventHook`은 그
  공백의 최후 방어선으로 유지.
- 스레드ID 불일치(ADR-0005, WinUI 패키지형 앱)는 `OnSetFocus`에도 구조적으로 재현되지만, 이미
  PID로 매칭하기로 한 기존 결정이 그대로 흡수 — 새 문제 아님.
- 구체 설계 제안: TIP은 `Activate()`에서 받은 `threadMgr`에 `ITfThreadMgrEventSink`도 추가로
  `AdviseSink`. `OnSetFocus(pdimFocus, ...)`에서 `pdimFocus != NULL`이면 "포커스 획득", `NULL`이면
  "포커스 상실"을 타임스탬프와 함께 IPC로 보고. UI는 `SetWinEventHook` 포그라운드 PID를 기본으로
  쓰되, 그 PID가 상태 테이블에 없거나 더 최근 "포커스 획득" 보고가 있는 PID가 있으면 그쪽을 우선.
- **미검증으로 남은 것**: `conhost.exe`에서 `OnSetFocus`가 실제로 발화하는지는 실측 안 됨(TSF 문서상
  `SetFocus` 호출은 "애플리케이션 협조적"이라 문서만으론 100% 보장 안 됨) — 구현 전 프로토타입
  단계에서 최우선 검증 항목으로 권고.
