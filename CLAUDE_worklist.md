# CLAUDE_worklist.md — 구현 작업 계획

`CLAUDE.md` 스펙을 기반으로 한 구현 작업 목록. 각 항목은 커밋 하나 분량으로 쪼갰고,
위에서부터 순서대로(우선순위대로) 진행한다. 완료하면 체크박스를 `[x]`로 표시한다.

각 항목에는 **검증방법** 줄을 두어 어떻게 확인할지 적는다. WinForms/COM 코드는 실제 창·COM
객체를 다루는 부분(글루 코드)과, 입력→출력이 정해진 순수 로직으로 나뉘는데 — 글루 코드는
실제 OS 상태가 있어야 의미가 있어 단위 테스트로 감싸는 실익이 적고, 순수 로직은 테스트부터
작성하는 TDD(`/tdd`)가 잘 맞는다. 그래서 항목마다 이 둘을 구분해 적는다.

## 0. 사전 확인

- [x] **개발 환경 확인**
  - 검증방법: `dotnet --version`/`dotnet --list-runtimes` 실행 결과를 직접 확인. TDD 대상
    아님(코드가 아니라 환경 점검). SDK `10.0.302`, `Microsoft.WindowsDesktop.App 8.0.13`
    (WinForms, net8.0-windows 대상에 필요) 설치 확인 완료 — 별도의 "개발환경 설정" 작업은
    필요 없음.

## 1. 뼈대

- [x] **프로젝트 스캐폴딩**
  - 검증방법: `dotnet build`가 성공하는지로 확인. TDD 대상 아님 — 프로젝트 생성/설정 파일이라
    "테스트할 동작" 자체가 없음. (CLAUDE.md 4, 7절)
- [x] **설정값(Constants) 모듈**
  - 검증방법: 코드 리뷰(값이 CLAUDE.md 5절과 일치하는지 눈으로 대조)로 충분. TDD 대상
    아님 — 상수 나열이라 동작이 없음. (CLAUDE.md 5절)

## 2. 인디케이터 UI (TSF 연동 이전 — 하드코딩 상태로 렌더링 검증)

- [x] **인디케이터 창 구현**
  - 검증방법: "상태(한글/영문) → 배경색/텍스트" 매핑 함수는 TDD로 진행 — 테스트를 먼저
    작성해 빨강/"한", 파랑/"A" 매핑을 검증한다. 반면 `CreateParams` 오버라이드로 만드는
    topmost·no-activate·tool window 속성은 실제 창을 띄워 눈으로 확인해야 하는 글루 코드라
    TDD 대상이 아니며, 앱을 실행해 수동으로 확인한다. (CLAUDE.md 5절)
- [x] **멀티 모니터 배치**
  - 검증방법: "모니터 경계 + 인디케이터 크기 + 상단 여백 → 창 위치(x, y)" 계산 함수는 TDD로
    진행 — 임의의 모니터 좌표를 입력으로 줘서 기대 좌표가 나오는지 테스트한다. `Screen.AllScreens`
    열거와 실제 폼 생성/배치는 글루 코드이므로 실제 모니터 환경에서 눈으로 확인한다.
    (CLAUDE.md 5절)
- [x] **클릭 반전 + 전체 동기화**
  - 검증방법: "현재 표시 상태 → 반전된 상태" 전이와 "모든 모니터에 같은 값 전파"는 UI 이벤트와
    분리된 상태 관리 클래스/함수로 뽑아 TDD로 진행 가능 — 클릭 이벤트 자체는 그 로직을
    호출하는 얇은 연결부라 실행해서 수동으로 클릭해보는 정도로 확인한다. (CLAUDE.md 6절)

## 3. TIP + IPC 연동 (ADR-0005)

- [x] **TIP DLL 뼈대 + 등록/로딩 검증**
  - `src/ImeIndicatorTip/` 프로젝트 생성(vcxproj), `DllRegisterServer`/`DllUnregisterServer`/
    `DllGetClassObject`/`DllCanUnloadNow` + 빈 `Activate`/`Deactivate`만 구현.
  - 검증방법: `regsvr32`로 등록 후 메모장을 완전히 종료했다가 새로 띄워 `Activate()`가
    호출되는지(임시 로그로) 확인 완료. **HKCU\Software\Classes만으로는 COM 등록은 성공해도
    TSF가 실제로 로드하지 않고, HKLM에 등록해야 로드된다는 것을 실측으로 확인** — 관리자
    권한이 필요하다는 뜻(ADR-0005 Update, CLAUDE.md 7절 반영 완료). `regsvr32 /u`로 등록
    해제 시 레지스트리가 깨끗이 정리되는지도 확인 완료. TDD 대상 아님 — COM 등록/로딩
    자체가 글루.
- [x] **스레드 스코프 컴파트먼트 구독 (TipPoc7 로직 이식)**
  - `ImeStateTip.cpp`에 TipPoc7의 스레드 스코프 QI + `GetCompartment` + `AdviseSink`
    로직을 그대로 옮겼다(포커스 추적용 `ITfThreadMgrEventSink`는 제외 — UI 프로세스 책임).
    아직 IPC는 연결하지 않고 임시 로그로만 확인.
  - 검증방법: 메모장에서 한/영 반복 전환하며 로그에 값이 0/1로 정확히 토글되는지 수동
    확인 완료 — `OnChange` 4회, 값 0→1→0→1로 실제 입력 패턴과 정확히 일치. TDD 대상
    아님 — 실제 TSF COM 콜백. (ADR-0005)
- [x] **IPC 클라이언트 (TIP 쪽)**
  - `IpcClient.h/.cpp`: 백그라운드 워커 스레드 + 논블로킹 mailbox +
    connect-write-disconnect 파이프 클라이언트(`\\.\pipe\ingee.ImeIndicator.StateReport`).
    `ImeStateTip`의 임시 로그 호출을 `IpcClient_ReportState`로 교체 완료.
  - 검증방법: 임시 PowerShell 파이프 서버로 실제 메시지(pid, isKoreanOpen)가 정확히
    도착하는지 확인 완료(입력 패턴과 일치하는 4개 메시지 수신). 서버를 끈 상태에서도
    메모장이 멈추거나 크래시하지 않고 계속 응답하는지도 확인 완료. TDD 대상 아님 — Win32
    파이프 I/O 글루. (CLAUDE.md 4절)
- [x] **UI 상태 판단 순수 로직 (TDD)**
  - `ForegroundStateResolver.Resolve(pid, table, lastKnown)` 테스트 먼저 작성 → 구현 완료
    (`ForegroundStateResolverTests.cs`, 5개 테스트: PID 있음/true/false, 없음(마지막 상태
    유지), 빈 테이블).
  - 검증방법: TDD로 진행, 글루 코드 없음. 전체 테스트 스위트 15개 통과. (CLAUDE.md 4절)
- [x] **UI IPC 리스너 + 포그라운드 추적 연결**
  - `ImeStateIpcListener`(파이프 서버 + PID→상태 테이블), `ForegroundWindowTracker`
    (`SetWinEventHook(EVENT_SYSTEM_FOREGROUND)` 단일 인스턴스) 구현. 둘 다 `Resolve` 호출 후
    `IndicatorStateStore.Set`으로 반영. UI 스레드 마샬링(`SynchronizationContext`) 포함.
  - 검증방법: 실제 TIP DLL 등록 상태로 메모장에서 한/영 전환 시 인디케이터 창이 실제로
    바뀌는지 수동 확인 완료(사용자 확인). TDD 대상 아님. (ADR-0005, CLAUDE.md 4절)
- [x] **Program.cs 재배선 + 구 TSF 코드 제거**
  - `TF_CreateThreadMgr`/`TsfImeStateMonitor` 구성 코드 제거, `ICompartmentReader.cs`/
    `TsfCompartmentReader.cs`/`TsfImeStateMonitor.cs` 삭제, `Vanara.PInvoke.TextServicesFramework`
    패키지 참조 제거. `ForegroundWindowTracker`/`ImeStateIpcListener` 생성으로 교체.
  - 검증방법: `dotnet build`/`dotnet test` 통과(15개 테스트 전부 통과) + 앱 실행 시 크래시
    없이 기본값(영문)으로 뜨는 것, TIP 등록 후 실제 상태 반영까지 확인 완료. TDD 대상
    아님 — 배선 변경. (참고: `dotnet build`는 `ImeIndicatorTip.vcxproj`를 함께 못 빌드하므로
    C#/C++ 프로젝트를 각각 따로 빌드해야 함 — `.slnx` 단일 빌드는 후속 과제로 남김.)
- [ ] **엔드투엔드 수동 시나리오 검증** — 진행 중, 완료 아님
  - 메모장(Win32), Notepad++, mintty, Windows 11 패키지형 메모장(WinUI)은 정확히 반영됨
    확인(새로 뜬 프로세스 기준). 하지만 **`cmd.exe`(클래식 콘솔)와 Excel에서 반영되지
    않는 문제를 발견 — 아직 미해결.** 모든 앱에서 정확히 표시되지 않으면 인디케이터
    자체의 목적(CLAUDE.md 3절 "오차 없이" 목표)을 달성하지 못하므로, 이 두 문제를 해결하기
    전까지는 이 항목을 완료로 볼 수 없다.
  - 원인 조사 결과는 ADR-0005 Update에 기록: (1) `cmd.exe`는 보이는 창 PID와 실제 TIP
    로드 프로세스(`conhost.exe`)가 달라 구조적으로 매칭 실패, (2) Excel은
    `GUID_TFCAT_TIP_KEYBOARD` 카테고리 등록 후에도 TIP `Activate()` 자체가 안 옴(원인 미규명).
  - 다음 시도할 것: `conhost.exe` 케이스는 포그라운드 프로세스가 콘솔 호스트 클라이언트일 때
    연결된 `conhost.exe`를 추가로 찾아 매칭하는 방법 검토. Excel은 더 깊은 진단(Process
    Monitor 급 도구, 또는 Office의 텍스트 서비스 통합 방식 추가 조사) 필요.

## 4. 트레이 · 시작프로그램

- [ ] **트레이 아이콘 및 컨텍스트 메뉴**
  - 검증방법: 트레이에 아이콘이 뜨는지, 참고 이미지(`docs/assets/tray-icon-reference.png`)
    스타일과 맞는지, 우클릭 메뉴의 "종료"가 동작하는지 실행해서 눈으로 확인. TDD 대상
    아님 — 에셋 제작과 `NotifyIcon` 연결이라 동작이라 부를 게 없음. (CLAUDE.md 7절)
- [ ] **시작프로그램 등록/해제**
  - 검증방법: "현재 등록 여부(bool) → 메뉴 라벨/체크 상태" 판단 로직은 TDD로 먼저 작성.
    실제 레지스트리 읽기/쓰기는 인터페이스로 감싸 가짜 구현으로 등록/해제 흐름까지 TDD로
    검증할 수 있다. 다만 실제 레지스트리 키가 제대로 생기고 로그인 시 실행되는지는 마지막에
    한 번 수동으로 확인한다. (CLAUDE.md 7절)

## 5. 마무리

- [ ] **방어적 처리 보강 및 전체 시나리오 점검**
  - 검증방법: 여러 앱을 오가며 실제 한/영 전환, 클릭 교정, 포커스 전환, (가능하면) COM 호출
    실패 상황까지 수동으로 시나리오를 돌려 크래시 없이 인디케이터가 유지되는지 확인. TDD
    대상 아님 — 이 항목 자체가 수동 검증 단계. (CLAUDE.md 4절)
- [ ] **Self-contained 단일 파일 게시 검증**
  - 검증방법: `dotnet publish -r win-x64 --self-contained -p:PublishSingleFile=true` 실행 후
    결과 exe를 별도 .NET 런타임 없는 환경(또는 그렇다고 가정하고)에서 실행해 정상 동작하는지
    확인. TDD 대상 아님 — 빌드/배포 절차 검증. (CLAUDE.md 7절)

## 마지막 세션 요약 (2026-08-07)

### 오늘 완료한 작업

- ADR-0005 신설(스레드 스코프 컴파트먼트 + PID 매칭), ADR-0004는 superseded로 표시.
- `CLAUDE.md` 4·7절을 TIP DLL + IPC + UI 쪽 포그라운드 PID 추적 아키텍처로 재작성,
  `CLAUDE_worklist.md` 3절을 그 아키텍처 기준 체크리스트로 재작성.
- `src/ImeIndicatorTip/`(C++ vcxproj, `ImeIndicator.slnx`에 통합) 신규 구현:
  - TIP DLL 뼈대(`DllExports`/`Registration`/`Guids`) — HKCU 등록은 COM은 되지만 TSF가
    로드 안 한다는 것, HKLM 등록해야 실제 로드된다는 것(=관리자 권한 필요)을 실측 확정.
  - `ImeStateTip.cpp`: `ITfThreadMgr`를 `GetGlobalCompartment()` 없이 직접
    `ITfCompartmentMgr`로 QI하는 스레드 스코프 구독(TipPoc7 로직 이식) — 메모장 실측으로
    검증 완료.
  - `IpcClient.h/.cpp`: 워커 스레드 + 논블로킹 mailbox + connect-write-disconnect named
    pipe(`\\.\pipe\ingee.ImeIndicator.StateReport`) 클라이언트. 서버 없어도 호스트 앱이
    멈추지 않는 것 확인.
- C# 쪽(`src/ImeIndicator/`) 신규/변경:
  - `ForegroundStateResolver`(TDD, 5개 테스트) — PID→상태 판단 순수 로직.
  - `ForegroundWindowTracker`(`SetWinEventHook(EVENT_SYSTEM_FOREGROUND)` 단일 인스턴스),
    `ImeStateIpcListener`(파이프 서버 + PID 테이블 + UI 스레드 마샬링) 신규.
  - `Program.cs` 재배선, 옛 TSF 코드(`ICompartmentReader`/`TsfCompartmentReader`/
    `TsfImeStateMonitor`, `Vanara.PInvoke.TextServicesFramework` 참조) 삭제.
  - 전체 테스트 15개 통과, 빌드 성공.
- 커밋 6개 완료(`c8c695b`~`38544d5`, 워크리스트 3절 항목 1~6 각각 하나씩).
- 엔드투엔드 수동 테스트 중 **중대한 미해결 문제 2건 발견**(아래 참고) — 사용자가 "모든
  앱에서 올바르게 표시되지 않으면 인디케이터 자체가 쓸모없다"고 명확히 지적, 이 문제
  해결이 다음 세션 최우선순위.

### 미완료 상태로 남은 작업과 현재 상태

- **워크리스트 3절 마지막 항목("엔드투엔드 수동 시나리오 검증")이 미완료.** 메모장(Win32),
  Notepad++, mintty, Windows 11 패키지형 메모장(새로 뜬 프로세스 기준)은 정확히 반영되지만:
  1. **`cmd.exe`(클래식 콘솔) 미반영** — 원인 규명됨: `GetForegroundWindow()`가 돌려주는
     보이는 창의 PID는 `cmd.exe` 자신이지만, 실제 TIP이 로드되고 정확한 값을 보고하는
     프로세스는 별도의 `conhost.exe`다. 둘의 PID가 달라 UI 쪽 PID 매칭이 구조적으로 실패.
     Windows Terminal·mintty처럼 자체 창을 그리는 터미널에는 해당 없음.
  2. **Excel 미반영** — 원인 미규명. 셀 편집 모드에서 실제 입력해도 TIP의 `Activate()`
     자체가 안 옴. `ITfCategoryMgr::RegisterCategory(GUID_TFCAT_TIP_KEYBOARD)` 카테고리
     등록을 추가해봤지만(다른 앱들은 이 카테고리 없이도 로드됐었음) 결과 동일 — 단순
     등록 누락이 아닌 것으로 보임. 이 카테고리 등록 코드 자체는 정당한 개선이라 유지.
  - 두 경우 다 크래시는 없음(CLAUDE.md 4절 "마지막 상태 유지" 방어 규칙대로 동작) — 다만
    화면에 틀린 상태가 계속 떠 있을 수 있다는 게 문제.
- **커밋 안 된 변경 있음**(`git status`): `CLAUDE.md`(3절에 "알려진 제약" 문단 추가),
  `CLAUDE_worklist.md`(이 요약 + 3절 마지막 항목 미완료로 수정), `docs/adr/0005-*.md`
  (Update 섹션 2개 추가 — 관리자 권한 확정 건, 엔드투엔드에서 발견한 제약 2건),
  `src/ImeIndicatorTip/Registration.cpp`(`GUID_TFCAT_TIP_KEYBOARD` 카테고리 등록 추가).
  다음 세션에서 커밋할지, 아니면 conhost/Excel 문제를 마저 해결한 뒤 한 번에 커밋할지
  판단 필요.
- TIP 레지스트리 등록은 세션 종료 시점에 확인 완료 — 등록 흔적 없음(`regsvr32 /u`까지
  전부 정리됨). 실행 중인 `ImeIndicator.exe`도 없음.

### 다음에 시작할 지점

1. **`conhost.exe` 매칭 문제부터 재개.** 중단된 지점: `cmd.exe`와 `conhost.exe`의
   부모/자식 프로세스 관계(`Win32_Process`의 `ParentProcessId`)를 확인해서 안정적으로
   매칭할 방법이 있는지 조사하려던 참이었음(PowerShell 명령 실행 직전에 세션 중단).
   - 확인할 것: `conhost.exe`가 `cmd.exe`의 자식인지 부모인지, 혹은 둘 다 아닌 제3의
     프로세스(csrss 등)의 자식으로 형제 관계인지. 관계가 안정적이면
     `ForegroundStateResolver`/`ImeStateIpcListener` 쪽에 "포그라운드 PID가 테이블에
     없고 콘솔 클래스 창(`GetClassName` == `"ConsoleWindowClass"`)이면, 관련
     `conhost.exe` PID들도 찾아서 그중 테이블에 있는 걸 대신 사용" 같은 폴백 로직을
     추가하는 방향 검토.
   - 대안도 열어둘 것: 프로세스 관계로 안정적으로 못 찾으면, PowerShell/도구로 실측
     가능한 다른 방법(`GetConsoleProcessList` 등)도 고려.
2. **Excel 문제는 더 깊은 진단 필요.** Process Monitor급 도구가 없다면, 적어도 Office의
   TSF 통합 방식에 대한 배경 조사(리버스 엔지니어링 대신 문서/커뮤니티 자료 조사)부터
   시작하는 게 나을 수 있음 — `ITfInputProcessorProfileMgr`를 통한 더 정식적인 활성화
   경로(`ActivateProfile`)가 필요한 건 아닌지 등.
3. 두 문제 중 하나라도 해결되면 그 즉시 워크리스트 3절 마지막 항목 재검증하고 완료 표시,
   커밋.

### 특이사항 / 참고

- **핵심 교훈(사용자 피드백)**: "알려진 제약"으로 문서화하고 다음으로 넘어가려 했다가
  거부당함 — 이 프로젝트에서 "모든 앱에서 정확히 표시"는 타협 불가능한 핵심 목표
  (CLAUDE.md 3절)이므로, 특정 앱 카테고리에서 안 되는 문제를 "제약"으로 남겨두고
  완료 처리하지 말 것. 완전히 해결하거나, 최소한 해결 시도를 계속할 것.
- TIP DLL 개발 중 흔한 마찰: 빌드한 DLL이 이미 로드해간 다른 프로세스(foobar2000, Edge,
  Epic Games Launcher 등)가 있으면 재빌드 시 `LNK1168`(쓰기용으로 열 수 없음)로 실패함.
  `/p:TargetName=<임시이름>`으로 다른 파일명으로 빌드해 우회하는 패턴을 계속 씀 — 최종
  정식 이름(`ImeIndicatorTip.dll`)으로 재빌드하려면 그 잠금 프로세스들을 언젠가 종료해야
  할 수 있음(사용자 앱이라 함부로 안 건드림).
- Windows 11 메모장은 단일 인스턴스라 "새로 띄운 창"이 기존 창 재사용일 수 있음 —
  테스트 때마다 `Get-Process notepad | Select MainWindowHandle`로 실제 새 프로세스인지
  확인하는 습관 필요.
- 시스템에 등록되는 자원 이름은 `ingee`로 시작하는 규칙 계속 적용 중(파이프 이름, TIP
  표시 이름). 이 규칙은 메모리에도 저장돼 있음.
- `dotnet build`가 `ImeIndicatorTip.vcxproj`를 함께 못 빌드함(C++ MSBuild 타깃 필요) —
  C#은 `dotnet build`/`dotnet test`로, C++은 `MSBuild.exe`로 각각 따로 빌드해야 함.
  `.slnx` 단일 명령 빌드는 아직 미해결 후속 과제.
- 커밋 메시지는 스킬 이름 언급 없이 실제 변경 내용 중심으로, 대화는 한글로 — 기존
  메모리 규칙 계속 적용 중.
