# 리서치: TSF 네이티브 포커스 신호(`ITfThreadMgrEventSink::OnSetFocus`)로 UI의 포커스 판단을 대체/보완할 수 있는가

- 대상 티켓: `.scratch/ime-focus-accuracy/issues/01-tsf-native-focus-signal.md`
- 조사일: 2026-08-08
- 방법: 1차 자료(MS Learn TSF API 레퍼런스, Windows Console 공식 문서, 공식 TSF 팀 블로그
  아카이브) 우선. 확정된 1차 자료가 없는 부분은 아키텍처 추론으로 명시하고 신뢰도를 별도
  표기했다.

## 요약 (TL;DR)

- **채택 권고: 병행(보완) — 완전 대체는 하지 않는다.**
- `ITfThreadMgrEventSink::OnSetFocus`는 우리 TIP이 이미 `Activate()`에서 받는 그 프로세스의
  스레드 스코프 `ITfThreadMgr`에 그대로 얹을 수 있는, 폴링이 필요 없는 콜백이다(신뢰도: 높음,
  MS Learn 1차 자료로 확인).
- `cmd.exe`/`conhost.exe` 불일치는 **conhost.exe가 콘솔 창의 "소유 PID/TID" 메타데이터를
  의도적으로 클라이언트 프로세스로 바꿔치기하는 문서화된 커널 오퍼레이션
  (`ConsoleControl(ConsoleSetWindowOwner, ...)`)** 때문일 가능성이 높다(신뢰도: 중상 —
  메커니즘의 존재는 1차 자료로 확인되지만, 그것이 이 프로젝트가 겪는 불일치의 직접 원인이라는
  인과관계 자체는 MS 문서가 명시적으로 확인해주지 않는 추론이다). TIP은 `GetCurrentProcessId()`로
  자기 프로세스 ID를 직접 얻으므로 이 창-소유권 바꿔치기의 영향을 받지 않는다 — 이것이 이
  신호가 유용한 핵심 이유다.
- 스레드ID 불일치(ADR-0005, WinUI 패키지형 앱) 문제는 `OnSetFocus`에도 구조적으로 동일하게
  존재하지만(같은 스레드 스코프 `ITfThreadMgr` 위에서 발생하므로), 이미 PID로 매칭하기로 한
  기존 결정과 그대로 호환된다 — 새로운 문제를 추가하지 않는다.
- 완전 대체를 권하지 않는 이유: `OnSetFocus`는 TIP이 `Activate()`된 프로세스에서만 발생한다.
  Excel처럼 애초에 `Activate()`조차 호출되지 않는 프로세스(티켓 02)에서는 이 신호 자체가
  존재하지 않으므로, `SetWinEventHook` 기반 포그라운드 추적을 완전히 걷어내면 그 경우 UI가
  포커스에 대해 아무 신호도 못 받는 상태가 된다.

---

## 1. `OnSetFocus`가 conhost.exe 같은 프로세스 불일치 상황에서도 신뢰성 있게 발생하는가

### 1.1 TSF 포커스 모델의 기본 메커니즘 (1차 자료)

- **스레드 매니저는 프로세스가 아니라 스레드 단위**로 만들어진다. 애플리케이션은
  `CoCreateInstance(CLSID_TF_ThreadMgr)`로, 텍스트 서비스(TIP)는
  `ITfTextInputProcessor::Activate`의 인자로 스레드 매니저를 얻는다.
  > "An application creates a thread manager object by calling CoCreateInstance with
  > CLSID_TFThreadMgr. ... A text service obtains a thread manager object in the text
  > service ITfTextInputProcessor::Activate method."
  — [Thread Manager (MS Learn, TSF)](https://learn.microsoft.com/en-us/windows/win32/tsf/thread-manager)

- `ITfThreadMgrEventSink`는 그 **동일한 `ITfThreadMgr` 인스턴스**에 `ITfSource::AdviseSink`로
  거는 이벤트 싱크다 — 우리 TIP이 이미 `GUID_COMPARTMENT_KEYBOARD_OPENCLOSE`를 구독할 때 쓰는
  것과 같은 패턴(`ITfSource` QI → `AdviseSink`)이며, 같은 `threadMgr` 포인터에 걸 수 있다.
  > "Target Interface: ITfThreadMgrEventSink | Hosting Interface: ITfThreadMgr"
  — [A Tour through TSF: Event sinks (공식 TSF 팀 블로그 아카이브, MS Learn)](https://learn.microsoft.com/en-us/archive/blogs/tsfaware/a-tour-through-tsf-event-sinks)

- `OnSetFocus`는 그 스레드 매니저 위에서 **문서 관리자(document manager) 포커스가 바뀔 때**
  호출된다.
  > "Called when a document view receives or loses the focus. ... pdimFocus: Pointer to the
  > document manager receiving the input focus. If no document is receiving the focus, this
  > will be NULL."
  — [ITfThreadMgrEventSink::OnSetFocus (MS Learn)](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfthreadmgreventsink-onsetfocus)

- 이 호출을 트리거하는 것은 `ITfThreadMgr::SetFocus`이며, **애플리케이션이 자기 창이
  입력 포커스를 받을 때 직접(혹은 `AssociateFocus`로 등록해두면 TSF 매니저가 대신) 호출해야
  한다**는 것이 명시돼 있다.
  > "The application must call this method when the document window receives the input
  > focus. If the application associates a window with a document manager using
  > ITfThreadMgr::AssociateFocus, the TSF manager calls this method for the application."
  — [ITfThreadMgr::SetFocus (MS Learn)](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfthreadmgr-setfocus)
  > "Associating the focus for a window with a document manager causes the TSF manager to
  > automatically call ITfThreadMgr::SetFocus with the associated document manager when the
  > associated window receives the focus."
  — [ITfThreadMgr::AssociateFocus (MS Learn)](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfthreadmgr-associatefocus)

**함의**: `OnSetFocus`는 OS 차원에서 보장되는 시스템 이벤트가 아니라, "그 프로세스 안의
애플리케이션(또는 TSF가 대신)이 `SetFocus`를 호출했을 때"만 발생하는 **애플리케이션 협조적
(cooperative) 신호**다. 다만 실무적으로는 `AssociateFocus` 경로를 쓰는 게 표준 패턴이라, 창이
실제 키보드 포커스를 받는 시점과 사실상 결합돼 있다. 이 결합의 강도(= 항상 1:1인지)를
MS 문서가 명시적으로 보장하지는 않는다 — "표준 구현이라면 이렇게 동작할 것"이라는 문서
기반 추론이다(신뢰도: 중상).

### 1.2 conhost.exe/cmd.exe 불일치의 구조적 원인 (근거 있는 추론)

티켓이 실측으로 재확인한 사실(`GetForegroundWindow` + `GetWindowThreadProcessId`로 확인한
`ConsoleWindowClass` 창의 소유 PID가 `cmd.exe` 자신)은, Windows Console 공식 문서에 있는
**의도적인 커널 오퍼레이션**과 정확히 들어맞는다:

> "Performs special kernel operations for console host applications. This includes
> **reparenting the console window**, allowing the console to pass foreground rights on to
> launched console subsystem applications and terminating attached processes."
>
> `ConsoleSetWindowOwner` — expects a pointer to `CONSOLEWINDOWOWNER { HWND hwnd; ULONG
> ProcessId; ULONG ThreadId; }`
— [ConsoleControl function (MS Learn, Windows Console 공식 문서)](https://learn.microsoft.com/en-us/windows/console/consolecontrol)

즉 `conhost.exe`는 콘솔 창의 "소유자로 보고되는" `ProcessId`/`ThreadId`를 커널 레벨에서
클라이언트 프로세스(`cmd.exe`) 쪽으로 명시적으로 바꿔치기할 수 있는 공식 오퍼레이션
(`ConsoleSetWindowOwner`)을 갖고 있다. 이는 `GetWindowThreadProcessId` 같은 외부 조회
API가 반환하는 값이 실제 메시지 펌프를 돌리는 프로세스와 다를 수 있다는 것을 뒷받침한다.
Console Host의 역할 정의 문서도 이 구조(server=conhost, client=cmd.exe)를 재확인한다:

> "The Windows Console Host, or conhost.exe, is both the server application for all of the
> Windows Console APIs as well as the classic Windows user interface for working with
> command-line applications."
— [Windows Console and Terminal Definitions (MS Learn)](https://learn.microsoft.com/en-us/windows/console/definitions)

**신뢰도 구분이 중요하다**:
- **높음(1차 자료로 직접 확인)**: `ConsoleSetWindowOwner`라는, 콘솔 창의 보고되는
  프로세스ID/스레드ID를 재설정하는 공식 커널 오퍼레이션이 *존재한다*.
- **중간(추론, MS가 명시적으로 확인 안 해줌)**: 이 프로젝트가 실측한 정확히 이 불일치가
  `ConsoleSetWindowOwner` 호출 *때문*이라는 인과관계. MS 문서는 언제/왜 이 오퍼레이션이
  호출되는지 세부를 공개하지 않는다. 다만 "콘솔 창을 taskbar/Alt-Tab에서 `cmd.exe`로
  보이게 하기 위한 호환성 장치"라는 용도(reparenting 목적 명시)는 이 프로젝트가 관찰한
  증상과 부합한다.
- 반대로, **TSF는 이 외부 API 우회 조회 경로와 무관하다**: 우리 TIP은 `conhost.exe`
  프로세스 안에서 COM으로 직접 로드되고, `Activate()`는 그 프로세스의 실제 스레드가
  호출한다. PID는 `GetWindowThreadProcessId`가 아니라 `GetCurrentProcessId()`로 그
  DLL 자신의 진짜 프로세스에서 직접 얻는다(`ImeStateTip.cpp:130` — 기존 코드에서 이미 이
  패턴을 씀). 즉 `ConsoleSetWindowOwner`가 무엇을 덮어쓰든, TIP 내부에서 얻는 PID는
  영향을 받지 않는다 — **이것이 이 신호를 채택할 실질적 이유**다.

### 1.3 conhost.exe에서 TIP이 실제로 Activate/구독되는지

ADR-0005의 엔드투엔드 실측(`docs/adr/0005-thread-scope-compartment-with-pid-matching.md`
Update 절)에서 이미 `conhost.exe`가 `GUID_COMPARTMENT_KEYBOARD_OPENCLOSE`를 정상적으로
보고한다는 게 확인돼 있다 — 즉 `conhost.exe`에서 TIP의 `Activate()`가 호출되고, 그 안에서
얻은 `ITfThreadMgr`가 유효하며 `OnChange` 콜백도 동작한다. `OnSetFocus`는 정확히 같은
`ITfThreadMgr` 인스턴스 위에 거는 것이므로 — 컴파트먼트 구독이 되는 프로세스라면 포커스
싱크도 걸 수 있다는 것은 인터페이스 요구사항 관점에서 새로운 위험이 없다(둘 다 같은
`ITfSource`를 제공하는 같은 객체 위에서 동작). 다만 이 프로젝트에서 `conhost.exe`에 대해
`OnSetFocus`가 **실제로 발화하는지는 아직 실측하지 않았다** — 1.1의 "애플리케이션 협조적"
특성상, `conhost.exe`가 내부적으로 `SetFocus`/`AssociateFocus`를 제대로 호출하는
구현인지는 문서만으로 100% 보장되지 않는다. **결론에 포함할 실행 항목**으로 남긴다(2절 참고).

---

## 2. 완전 대체 가능한가, 병행(폴백)해야 하는가

**권고: 병행. `SetWinEventHook` 기반 포그라운드 추적은 유지하고, TSF 포커스 신호를 우선
신호(더 정확할 때 덮어쓰는 소스)로 추가한다.**

근거:

1. **커버리지 공백이 다르다.** `OnSetFocus`는 TIP이 `Activate()`된 프로세스에서만 존재할 수
   있는 신호다. 티켓 02(`Excel` TIP 활성화 자체가 안 되는 문제)가 다루는 프로세스군에서는
   `Activate()`가 안 불리므로 `OnSetFocus`도 원천적으로 없다 — `ITfThreadMgrEventSink`는
   `ITfThreadMgr`가 있어야 걸 수 있고, `ITfThreadMgr`는 `Activate()`의 인자로만 받으므로
   ([Thread Manager, MS Learn](https://learn.microsoft.com/en-us/windows/win32/tsf/thread-manager)),
   `Activate()` 자체가 없으면 이 신호도 없다. `SetWinEventHook`은 TSF와 완전히 무관한
   OS 레벨 신호라 이 공백에서도 최소한 "어떤 프로세스가 전경인지"는 계속 알려준다(정확한
   IME 상태까지는 못 주더라도, CLAUDE.md 4절의 "마지막으로 알려진 상태 유지" 방어 규칙이
   이미 이 케이스를 커버한다).
2. **정확히 티켓이 겨냥한 실패 모드(conhost/cmd.exe)에서는 `SetWinEventHook`의 결과가
   구조적으로 쓸모없다** — `cmd.exe`의 PID는 애초에 어떤 TIP도 보고한 적 없는 PID이므로
   상태 테이블 조회가 항상 실패한다. 이것이 바로 티켓이 제안한 폴백 조건("포커스 PID가
   상태 테이블에 없을 때만 TSF 포커스 신호로 폴백")과 정확히 맞아떨어진다 — 별도의
   특수 케이스 분기 없이, 기존 "테이블에 없으면 폴백"이라는 일반 규칙 하나로
   `conhost.exe` 케이스가 자연스럽게 해결된다.
3. **완전 대체 시 리스크**: IPC 연결 실패, TIP 미등록 프로세스, 신호 유실 등 CLAUDE.md
   4절이 이미 상정한 방어 대상들이 전부 "포커스 신호 자체의 부재"로 이어진다.
   `SetWinEventHook`은 이런 실패와 독립적인 별도 채널이므로, 병행 구조가 단일 장애점을
   줄인다(가용성 관점의 일반 원칙 — TSF 관련 1차 자료가 이 트레이드오프를 직접 언급하진
   않으므로 이 항목은 아키텍처 판단으로 표기).

구체적 규칙 제안:
- TIP은 `OnSetFocus(pdimFocus, pdimPrevFocus)`에서 `pdimFocus != NULL`이면 자기 PID를
  "포커스 획득"으로, `pdimFocus == NULL`이면 "포커스 상실"로 IPC 보고한다.
- UI는 `SetWinEventHook`으로 얻은 포그라운드 PID를 여전히 1차 조회 키로 쓰되, **가장 최근에
  "포커스 획득"을 보고한 PID가 있고 그 보고 시각이 `SetWinEventHook` 이벤트보다 최신이면
  그 PID를 우선**한다 — 이 정도 조율 규칙이면 `conhost.exe`처럼 `SetWinEventHook`이 못 보는
  프로세스를 TSF 신호가 메꾼다.

---

## 3. 스레드ID 불일치(ADR-0005, WinUI 패키지형 앱)가 `OnSetFocus`에도 재현되는가

**재현되지만, 이미 PID로 매칭하기로 한 기존 결정이 그대로 흡수한다 — 새 문제는 아니다.**

- `OnSetFocus`가 걸리는 `ITfThreadMgr`는 우리 TIP이 `Activate(threadMgr, ...)`에서 받은
  바로 그 인스턴스다. ADR-0005가 이미 실측했듯, "TIP이 활성화되는 스레드"와 "창을 소유하는
  스레드"가 다른 앱(Windows 11 패키지형 메모장 등)이 있다 — 이는 어느 스레드의
  `ITfThreadMgr`에 TSF가 TIP을 Activate시켰는지의 문제이므로, 그 위에 거는 `OnSetFocus`
  싱크도 동일한 스레드 스코프에 묶인다(둘 다 같은 `threadMgr` 포인터를 쓰기 때문에
  구조적으로 분리될 수 없다 — 이 부분은 프로젝트 코드 구조[`ImeStateTip.cpp`]와 MS Learn
  Thread Manager 문서를 결합한 논리적 귀결이지, 별도의 1차 자료가 "스레드 불일치가
  OnSetFocus에도 재현된다"고 직접 서술하지는 않는다. 신뢰도: 중상, 강한 논리적 추론).
- 하지만 우리가 이미 매칭 키로 쓰기로 확정한 것은 **스레드ID가 아니라 PID**다(ADR-0005:
  "매칭 키는 PID로 정한다"). `OnSetFocus` 콜백 안에서 PID를 얻는 방법도 기존 컴파트먼트
  보고와 동일하게 `GetCurrentProcessId()`이며, 이 값은 **그 프로세스 안 어느 스레드에서
  콜백이 오든 동일**하다. 따라서 "TIP 활성화 스레드 ≠ 창 소유 스레드"라는 사실 자체는
  `OnSetFocus`에도 그대로 남아 있지만, PID 매칭이라는 기존 설계가 애초에 스레드ID를
  안 보므로 실질적 영향이 없다.
- 남는 리스크는 스레드가 아니라 **프로세스 경계**다: 만약 어떤 앱이 (지금까지 관찰된
  범위 밖에서) 텍스트 입력을 처리하는 스레드가 아예 다른 프로세스에 있다면(=지금 conhost
  사례처럼) 이 신호도 그 "실제 프로세스"의 PID로만 보고된다 — 이는 새로운 실패 모드가
  아니라 오히려 1절에서 설명한, 이 신호를 채택하는 이유 그 자체다.

---

## 4. 폴링 없이 구독 가능한가 (CLAUDE.md "폴링 회피" 원칙과의 부합)

**부합한다. `OnSetFocus`는 순수 콜백/이벤트 싱크이며 폴링이 필요 없다.**

- 설치 방법 자체가 기존에 이미 쓰고 있는 `ITfCompartmentEventSink`와 완전히 같은 패턴이다:
  호스팅 인터페이스(`ITfThreadMgr`)를 `ITfSource`로 QI하고 `AdviseSink`로 건다.
  > "Event sinks are installed by a two-step process: 1) Call QueryInterface() on a host
  > interface for ITfSource; 2) Call ITfSource::AdviseSink() with the target interface IID
  > and the target interface pointer."
  — [A Tour through TSF: Event sinks (MS Learn, 공식 TSF 팀 블로그 아카이브)](https://learn.microsoft.com/en-us/archive/blogs/tsfaware/a-tour-through-tsf-event-sinks)
- `ITfThreadMgrEventSink`도 이 표에 `ITfThreadMgr`가 호스팅 인터페이스로 명시된 동일
  패밀리의 인터페이스다 — 폴링 타이머가 개입할 지점이 없다.
- 참고로 커뮤니티(비1차) 자료인 Microsoft Q&A 스레드도 `GUID_COMPARTMENT_KEYBOARD_OPENCLOSE`
  + `ITfCompartmentEventSink` 조합을 "폴링보다 우월한 이벤트 기반 접근"으로 설명하고 있어
  이 프로젝트의 기존 원칙과 일치한다(신뢰도: 낮음~중간 — 커뮤니티 답변, 참고용).
  — [How to reliably get IME Open/Close state using TSF (Microsoft Q&A)](https://learn.microsoft.com/en-my/answers/questions/5863957/how-to-reliably-get-ime-open-close-state-using-tsf)

---

## 5. 결론 및 권고

### 채택 여부

**채택한다 — 단, 완전 대체가 아니라 보완(우선순위가 더 높은 추가 신호)으로.**

### 채택 시 TIP의 책임 변화

- 기존: `Activate()`에서 스레드 스코프 `GUID_COMPARTMENT_KEYBOARD_OPENCLOSE`를 구독하고,
  값이 바뀔 때(`OnChange`)만 `(PID, 상태)`를 IPC로 보고.
- 추가: 같은 `Activate(threadMgr, ...)`에서 받은 `threadMgr`를 `ITfSource`로 QI해
  `ITfThreadMgrEventSink`(`OnSetFocus`)도 `AdviseSink`로 건다. `OnSetFocus(pdimFocus, ...)`가
  호출되면 `pdimFocus != NULL`일 때 "이 PID가 포커스를 얻음", `NULL`일 때 "이 PID가 포커스를
  잃음"을 타임스탬프와 함께 IPC로 보고한다(기존 `ImeStateTip.cpp`의
  `SubscribeThreadScopeCompartment`와 대칭되는 `SubscribeThreadFocusSink` 정도의 함수로
  구현 가능 — 같은 `threadMgr` 포인터, 같은 `ITfSource` QI 패턴 재사용).
- `Deactivate()`에서 컴파트먼트 언싱크와 동일하게 이 싱크도 `UnadviseSink`.

### 채택 시 UI의 책임 변화

- 기존: `SetWinEventHook(EVENT_SYSTEM_FOREGROUND)`로 얻은 포그라운드 PID로 상태 테이블을
  조회, 없으면 마지막 상태 유지.
- 추가: IPC로 들어오는 "포커스 획득/상실" 보고를 PID별 타임스탬프로 별도 유지한다.
  표시할 PID를 고를 때: `SetWinEventHook`의 최신 포그라운드 PID를 기본으로 쓰되, 그 PID가
  상태 테이블에 없거나(예: `cmd.exe`) 그 PID보다 더 최근에 "포커스 획득"을 보고한 다른
  PID가 있으면 그쪽을 우선한다. `SetWinEventHook` 자체는 제거하지 않는다(Excel류처럼
  TIP이 아예 없는 프로세스에 대한 최후 보루이자, TSF 포커스 신호가 아직 도착하지 않은
  시작 직후 구간의 초기값 소스로 계속 필요).

### 확인되지 않은 채 남은 것 (실행 전 검증 권장)

- `conhost.exe`에서 `OnSetFocus`가 실제로 발화하는지는 이번 조사에서 1차 자료로 확정하지
  못했다(1.3절) — MS 문서상 `SetFocus` 호출은 "애플리케이션 협조적"이라, `conhost.exe`
  내부 구현이 이를 제대로 트리거하는지는 실측이 필요하다. 프로토타입 단계에서 가장 먼저
  검증해야 할 항목으로 권고한다.
- `ConsoleSetWindowOwner`가 이 프로젝트가 겪는 정확한 증상의 원인이라는 인과관계는 추론이다
  (1.2절 신뢰도 표기 참고) — 다만 이 인과관계가 틀리더라도, "TIP이 자기 프로세스 안에서
  `GetCurrentProcessId()`로 직접 PID를 얻는다"는 사실 자체는 변하지 않으므로 권고안의
  유효성에는 영향이 없다.

---

## 출처 목록 (신뢰도순)

**1차 자료 — 높음**
- [ITfThreadMgrEventSink::OnSetFocus — MS Learn](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfthreadmgreventsink-onsetfocus)
- [ITfThreadMgrEventSink 인터페이스 — MS Learn](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nn-msctf-itfthreadmgreventsink)
- [ITfThreadMgr::SetFocus — MS Learn](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfthreadmgr-setfocus)
- [ITfThreadMgr::AssociateFocus — MS Learn](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfthreadmgr-associatefocus)
- [Thread Manager (TSF 개념 문서) — MS Learn](https://learn.microsoft.com/en-us/windows/win32/tsf/thread-manager)
- [Architecture (TSF 개념 문서) — MS Learn](https://learn.microsoft.com/en-us/windows/win32/tsf/architecture)
- [ConsoleControl 함수 (ConsoleSetWindowOwner 포함) — MS Learn Windows Console 공식 문서](https://learn.microsoft.com/en-us/windows/console/consolecontrol)
- [Windows Console and Terminal Definitions — MS Learn](https://learn.microsoft.com/en-us/windows/console/definitions)

**1차에 준함 — 공식 아카이브, 높음**
- [A Tour through TSF: Event sinks — MS Learn 아카이브(옛 TSF 팀 공식 블로그)](https://learn.microsoft.com/en-us/archive/blogs/tsfaware/a-tour-through-tsf-event-sinks)

**참고 — 낮음~중간 (커뮤니티/2차)**
- [ITfThreadFocusSink 인터페이스 — MS Learn](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nn-msctf-itfthreadfocussink)
  (참고용으로만 조사 — `req.dll: Tiptsf.dll`로 표기돼 핵심 `msctf.dll`의 `OnSetFocus`와
  달리 별도 TSF 1.0 재배포 패키지 계열 문서일 가능성이 있어 이번 권고에서는 채택하지 않음)
- [How to reliably get IME Open/Close state using TSF — Microsoft Q&A](https://learn.microsoft.com/en-my/answers/questions/5863957/how-to-reliably-get-ime-open-close-state-using-tsf)
  (커뮤니티 답변, conhost/focus 이벤트는 다루지 않음 — 컴파트먼트+이벤트싱크 패턴이
  폴링보다 낫다는 일반 원칙만 참고)

**프로젝트 내부 근거 (외부 1차 자료는 아니지만 결론의 전제)**
- `docs/adr/0005-thread-scope-compartment-with-pid-matching.md` — conhost.exe 컴파트먼트
  보고 실측, PID 매칭 결정, 스레드ID 불일치 실측
- `src/ImeIndicatorTip/ImeStateTip.cpp` — 기존 `Activate`/`ITfSource` 구독 패턴,
  `GetCurrentProcessId()` 사용 확인
