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
- [ ] **스레드 스코프 컴파트먼트 구독 (TipPoc7 로직 이식)**
  - `ImeStateTip.cpp`에 TipPoc7의 스레드 스코프 QI + `GetCompartment` + `AdviseSink`
    로직을 그대로 옮긴다. 아직 IPC는 연결하지 않고 임시 로그로만 확인.
  - 검증방법: 메모장에서 한/영 반복 전환하며 로그에 값이 0/1로 정확히 토글되는지 수동
    확인. TDD 대상 아님 — 실제 TSF COM 콜백. (ADR-0005)
- [ ] **IPC 클라이언트 (TIP 쪽)**
  - `IpcClient.h/.cpp`: 백그라운드 워커 스레드 + 논블로킹 mailbox +
    connect-write-disconnect 파이프 클라이언트(`\\.\pipe\ingee.ImeIndicator.StateReport`).
    `ImeStateTip`의 로그 호출을 `IpcClient_ReportState`로 교체.
  - 검증방법: 파이프 서버(UI) 없이 앱을 켜도 멈추거나 크래시 안 하는지, 서버가 있을 때
    실제로 메시지가 도착하는지 수동 확인. TDD 대상 아님 — Win32 파이프 I/O 글루.
    (CLAUDE.md 4절)
- [ ] **UI 상태 판단 순수 로직 (TDD)**
  - `ForegroundStateResolver.Resolve(pid, table, lastKnown)` 테스트 먼저 작성 → 구현.
  - 검증방법: TDD로 진행, 글루 코드 없음. (CLAUDE.md 4절)
- [ ] **UI IPC 리스너 + 포그라운드 추적 연결**
  - `ImeStateIpcListener`(파이프 서버 + PID→상태 테이블), `ForegroundWindowTracker`
    (`SetWinEventHook(EVENT_SYSTEM_FOREGROUND)` 단일 인스턴스) 구현. 둘 다 `Resolve` 호출 후
    `IndicatorStateStore.Set`으로 반영. UI 스레드 마샬링(`SynchronizationContext`) 포함.
  - 검증방법: 실제 TIP DLL 등록 상태로 여러 앱 사이를 오가며 한/영 상태가 정확히
    반영되는지 수동 확인. TDD 대상 아님. (ADR-0005, CLAUDE.md 4절)
- [ ] **Program.cs 재배선 + 구 TSF 코드 제거**
  - `TF_CreateThreadMgr`/`TsfImeStateMonitor` 구성 코드 제거, `ICompartmentReader.cs`/
    `TsfCompartmentReader.cs`/`TsfImeStateMonitor.cs` 삭제, `Vanara.PInvoke.TextServicesFramework`
    패키지 참조 제거. `ForegroundWindowTracker`/`ImeStateIpcListener` 생성으로 교체.
  - 검증방법: `dotnet build`/`dotnet test` 통과 + 앱 실행 시 크래시 없이 기본값(영문)으로
    뜨는지 확인. TDD 대상 아님 — 배선 변경.
- [ ] **엔드투엔드 수동 시나리오 검증**
  - 검증방법: 메모장(Win32), Windows 11 패키지형 메모장(WinUI), 터미널 등 서로 다른 종류의
    앱을 오가며 한/영 전환·포커스 전환·클릭 교정을 실제로 반복해 크래시 없이 정확히
    반영되는지 확인. TDD 대상 아님. (ADR-0005, CLAUDE.md 4·6절)

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
