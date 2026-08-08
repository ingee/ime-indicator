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
    않는 문제를 발견** — 원인 조사는 ADR-0005 Update, 해결 방향 결정은 wayfinder 맵
    (`.scratch/ime-focus-accuracy/`) 거쳐 **ADR-0006**으로 확정됨(2026-08-08). 아래 하위
    항목들을 구현·검증해야 이 항목을 완료로 볼 수 있다.

- [ ] **TIP: TSF 포커스 신호(`OnSetFocus`) 구독 + IPC 보고 (ADR-0006)**
  - `ImeStateTip.cpp`에 `ITfThreadMgrEventSink`(`OnSetFocus`) 구독 추가 — 컴파트먼트 구독과
    같은 `threadMgr`에 `AdviseSink`. `pdimFocus != NULL`이면 "포커스 획득", `NULL`이면
    "포커스 상실"을 타임스탬프와 함께 `IpcClient`로 보고(메시지 포맷 확장 필요).
  - 검증방법: 메모장 등 기존에 정상 동작하던 앱에서 포커스 전환 시 새 메시지가 오는지,
    기존 컴파트먼트 보고를 깨뜨리지 않는지 수동 확인. **`conhost.exe`(`cmd.exe` 실행 후)에서
    `OnSetFocus`가 실제로 발화하는지가 이 항목의 핵심 검증 포인트**(리서치에서 미검증으로
    남김 — `.scratch/ime-focus-accuracy/issues/01-tsf-native-focus-signal.md`) — 여기서
    발화 안 하면 ADR-0006 자체를 재검토해야 하므로 최우선으로 확인한다. TDD 대상 아님 —
    실제 TSF COM 콜백.
- [ ] **UI: 포커스 신호 병합 우선순위 로직 (TDD)**
  - "`SetWinEventHook` PID + 상태 테이블 + 포커스 신호(PID·타임스탬프) → 표시할 PID" 판단을
    순수 함수로 뽑아 TDD로 진행(`ForegroundStateResolver` 확장 또는 별도 함수). 테이블에
    그 PID가 없거나, 더 최근 "포커스 획득"을 보고한 다른 PID가 있으면 그쪽을 우선한다.
  - 검증방법: TDD로 우선순위 규칙(테이블에 있음/없음 × 포커스 신호 있음/없음/더 최신임)을
    표로 짜서 테스트. `ImeStateIpcListener`에 연결하는 배선 자체는 글루.
- [ ] **`cmd.exe` 반영 확인**
  - 위 두 항목 구현 후, `cmd.exe`에서 한/영 전환 시 인디케이터가 정확히 반영되는지 수동
    확인. TDD 대상 아님.
- [ ] **Excel: `TF_IPPMF_ENABLEPROFILE` 저위험 실험 (ADR-0006)**
  - `Registration.cpp`(또는 별도 초기화 경로)에서 `ITfInputProcessorProfileMgr::RegisterProfile`
    + `ActivateProfile(..., TF_IPPMF_ENABLEPROFILE)` 호출을 추가한다 — 선택 전환
    (`TF_IPPMF_FORPROCESS`/`FORSESSION`)은 절대 쓰지 않는다(CLAUDE.md 3절 비목표).
  - 검증방법: Excel 셀 편집 중 한/영 전환 시 `Activate()`가 호출되는지 수동 확인. **성공하면**
    아래 재검증 항목으로 넘어간다. **실패하면 이 항목을 완료 처리하지 않고, 코드는 롤백하지
    말고 그대로 둔 채 사용자와 다음 방안을 상의한다**(ADR-0006 — 실패해도 자동으로 "알려진
    제약"으로 확정하지 않음). TDD 대상 아님 — 실제 TSF 등록/활성화 글루.
- [ ] **엔드투엔드 수동 시나리오 재검증**
  - 위 항목들이 끝나면(Excel은 성공했거나, 상의 후 별도 결론이 났으면) 메모장·Notepad++·
    mintty·패키지형 메모장·`cmd.exe`·Excel을 다시 한 번씩 오가며 전부 정확히 반영되는지
    최종 확인한다. 전부 통과해야 이 항목과 위 "엔드투엔드 수동 시나리오 검증" 항목을 모두
    완료로 표시한다.

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

## 마지막 세션 요약 (2026-08-08)

### 오늘 완료한 작업

이번 세션은 **순수하게 결정 단계**였다 — 코드 구현은 하나도 하지 않았고, `cmd.exe`/Excel
미반영 문제(전 세션에서 미해결로 남김)를 어떻게 해결할지 조사하고 확정하는 데 집중했다.

- 실측으로 ADR-0005의 기존 진단을 재확인: `cmd.exe`를 강제 포그라운드에 놓고
  `GetForegroundWindow`/`GetWindowThreadProcessId`를 직접 호출한 결과, 보이는 콘솔 창의
  소유 PID는 여전히 `conhost.exe`가 아니라 `cmd.exe` 자신이었다(조사 중 나온 "사실은 우리
  코드 버그일 수도 있다"는 가설을 기각).
- 사용자가 "앱마다 특이 케이스를 패치하는 접근이 빈틈이 많다"고 지적 → "Windows 자체
  입력 표시기는 모든 앱에서 어떻게 정확히 동작하는가"를 근본 질문으로 재설정.
  Background agent로 조사한 결과(Google Project Zero, Tavis Ormandy 2019 등), Windows
  자체도 "프로세스마다 클라이언트 주입 + 중앙 relay" 패턴을 쓴다는 것을 확인 — 우리
  TIP 주입 아키텍처 자체는 정당했고, 문제는 "UI가 포커스를 판단하는 방법"에 있다는 쪽으로
  좁혀졌다.
- 사용자가 Windows 입력 표시기 스크린샷 2장 제공(`win-ime.png`: 정상 상태, `win-ime2.png`:
  포커스 없을 때의 "X" 상태) — 후자는 검토 후 "제3의 UI 상태" 후보로 논의했지만 구현
  용이성을 이유로 기각(현재 "마지막 상태 유지" 방식 그대로 유지, CLAUDE.md 5절 변경 없음).
- 사용자가 Excel(Office 2010)에서 직접 실측: 셀 편집 중에도 Windows 자체 입력 표시기는
  정확히 전환됨을 확인 — 이걸로 "Excel은 TSF를 아예 안 탄다"는 가설이 기각되고, "Excel도
  TSF는 타지만 우리 TIP만 `Activate()`가 안 된다"는 훨씬 좁고 풀 만한 문제로 재정의됨.
- `/wayfinder` 스킬로 destination을 그릴링(Q1~Q6)으로 확정하고, wayfinder 맵
  `.scratch/ime-focus-accuracy/`를 만들어 리서치 티켓 2개를 병렬 background subagent로
  조사·해결:
  - 티켓 01(`issues/01-tsf-native-focus-signal.md`): TIP이 `ITfThreadMgrEventSink::OnSetFocus`도
    함께 구독해 IPC로 포커스 획득/상실을 보고하는 안 — 채택(병행/보완, `SetWinEventHook`
    완전 대체 아님). 전문은 `docs/research/tsf-native-focus-signal.md`
    (브랜치 `research/tsf-native-focus-signal`, 커밋 `f1c88b0`).
  - 티켓 02(`issues/02-office-tip-activation-gap.md`): Excel이 우리 TIP을 왜 `Activate()`
    안 하는지 — 근본 원인 미확정(Excel 내부 비공개), `TF_IPPMF_ENABLEPROFILE` 저위험 실험
    권고. 전문은 `docs/research/office-tip-activation-gap.md`
    (브랜치 `research/office-tip-activation-gap`, 커밋 `0d59687`).
  - 두 subagent가 만든 리서치 브랜치가 프로젝트 실제 히스토리와 무관한 orphan 커밋을
    베이스로 만들어져 있어서, 임시 worktree로 각각 `feature/ime-state-detection` 위에
    다시 커밋해 바로잡았다(subagent의 worktree 격리 정책 한계로 보임 — 다음에 비슷한 작업
    시킬 때 참고).
  - 사용자가 두 결론을 확정하되 한 가지 수정: Excel 실험이 실패해도 리서치가 제안한 "알려서
    제약으로 수용"을 자동 적용하지 않고, **반드시 다시 상의**하기로 함(전 세션의 "알려진
    제약으로 넘어가는 것 거부" 전례와 일치시킴).
  - 사용자가 "`OnSetFocus`를 쓰면 `SetWinEventHook`은 없애도 되지 않냐"고 재확인 질문 →
    Excel처럼 TIP이 `Activate()` 안 되는 프로세스에서는 `OnSetFocus` 자체가 없으므로
    `SetWinEventHook`을 최후 방어선으로 유지하기로 재확인(이미 문서에 반영된 결정과 일치).
- **`docs/adr/0006-tsf-focus-signal-and-office-activation-experiment.md`** 신설 — 위 결정
  전체를 기록.
- `CLAUDE.md` 갱신: 3절 "알려진 제약" 문단을 ADR-0006 참조로 재작성(Excel 실패 시 자동
  수용 안 한다는 문구 명시), 4절에 `OnSetFocus` 구독·병합 우선순위 규칙·Excel 실험 관련
  bullet 추가.
- `CLAUDE_worklist.md` 3절의 미완료 항목("엔드투엔드 수동 시나리오 검증")을 구체적 하위
  구현 항목 5개로 재작성(바로 아래 "다음에 시작할 지점" 참고).
- 커밋 1개(`d893882`, "docs: 포커스 인식 아키텍처 결정 — TSF 포커스 신호 병행 + Excel
  저위험 실험 (ADR-0006)") + `feature/ime-state-detection`,
  `research/tsf-native-focus-signal`, `research/office-tip-activation-gap` 3개 브랜치 모두
  origin에 push 완료. `prototype/tip-detection-poc-throwaway`는 이미 이전 세션에 push돼
  있었음을 확인(추가 조치 불필요).

### 미완료 상태로 남은 작업과 현재 상태

- 워크리스트 3절은 여전히 미완료 — 다만 이제 **막연한 "원인 규명 필요" 상태가 아니라
  구체적인 구현 계획 5개 항목**으로 바뀌었다:
  1. TIP: `OnSetFocus` 구독 + IPC 보고 (미착수)
  2. UI: 포커스 신호 병합 우선순위 로직, TDD (미착수)
  3. `cmd.exe` 반영 확인 (미착수 — 1·2 완료 후)
  4. Excel: `TF_IPPMF_ENABLEPROFILE` 저위험 실험 (미착수)
  5. 엔드투엔드 수동 시나리오 재검증 (미착수 — 1~4 완료 후)
- 코드베이스 자체는 전 세션 상태 그대로다(이번 세션은 문서/결정만 다뤘음) — working tree
  clean, `feature/ime-state-detection`은 origin과 동기화됨.

### 다음에 시작할 지점

1. **워크리스트 3절의 첫 번째 미완료 항목("TIP: TSF 포커스 신호 구독 + IPC 보고")부터
   재개.** 시작 전에 `src/ImeIndicatorTip/ImeStateTip.cpp`를 먼저 읽어서 기존 컴파트먼트
   구독 함수(리서치 문서가 "`SubscribeThreadScopeCompartment`"로 지칭한 것으로 보이는
   함수)의 정확한 구조와 `Activate()`에서 받는 `threadMgr` 포인터의 정확한 변수명을 확인할
   것 — 이번 세션에서 이 파일을 직접 열어보지 않아 정확한 줄 번호는 모른다.
   `ITfThreadMgrEventSink`(`OnSetFocus`)를 같은 `threadMgr`에 `AdviseSink`로 추가 구독하고,
   `Deactivate()`에서 짝을 맞춰 `UnadviseSink`할 것(기존 컴파트먼트 언싱크 패턴과 대칭).
2. IPC 메시지 포맷 확장이 필요하다 — 현재 `IpcClient.h/.cpp`(TIP 쪽)와
   `ImeStateIpcListener.cs`(UI 쪽, `MessageSize = 5`로 하드코딩된 `uint32 pid + uint8
   isKoreanOpen`)는 상태 보고 전용 고정 포맷이다. 포커스 획득/상실 보고(PID + 타임스탬프 +
   메시지 종류)를 함께 실어야 하므로, 메시지 종류를 구분하는 필드(예: 맨 앞 1바이트 태그)를
   추가하는 하위 호환 없는 프로토콜 변경이 필요 — 양쪽을 동시에 고쳐야 한다.
3. `OnSetFocus`가 `conhost.exe`에서 실제로 발화하는지가 이번 결정 전체의 핵심 전제다
   (리서치가 미검증으로 남김) — 구현 후 가장 먼저 `cmd.exe`를 띄워 실측 확인할 것. 여기서
   발화 안 하면 ADR-0006을 재검토해야 한다.
4. UI 쪽 병합 우선순위 로직은 `ForegroundStateResolver.cs`를 확장하거나 별도 함수로 TDD
   먼저 작성 — 표는 CLAUDE_worklist.md 3절의 해당 항목에 이미 정리돼 있다(테이블에
   있음/없음 × 포커스 신호 있음/없음/더 최신임의 조합).

### 특이사항 / 참고

- **핵심 결정 하나만 남긴다면**: `SetWinEventHook`은 없애지 않는다. `OnSetFocus`는 TIP이
  `Activate()`된 프로세스에서만 존재할 수 있는 신호라, Excel처럼 애초에 TIP이 로드조차
  안 되는 프로세스(현재도, 앞으로 발견될 수도 있는)의 최후 방어선으로 `SetWinEventHook`이
  계속 필요하다.
- **사용자 피드백(재확인)**: "실패하면 알려진 제약으로 조용히 수용"하는 패턴은 이 프로젝트에서
  거부된다 — Excel 실험이 실패해도 자동으로 문서화하고 넘어가지 말고 반드시 다시 상의할 것
  (ADR-0006, CLAUDE.md 3절에 명시).
- **subagent 사용 시 주의**: `isolation: "worktree"`로 띄운 research subagent가 브랜치를
  만들 때, 다른 브랜치(`feature/ime-state-detection`)의 파일 트리를 베이스로 체크아웃하는
  걸 거부하고 대신 훨씬 오래된 orphan 커밋을 베이스로 삼는 경우가 있었다(2번 다 그랬음,
  우연이 아니라 격리 정책으로 보임). 결과물(파일 diff)은 정상이었지만 브랜치의 부모 커밋이
  잘못됐으므로, 매번 `git show <subagent 커밋>:<path>`로 파일만 뽑아 임시 worktree에서
  `feature/ime-state-detection` 위에 다시 커밋하는 보정이 필요했다. 다음에도 같은 패턴이
  재현될 가능성이 높으니 미리 감안할 것.
- **메모리 갱신**: 권한 프롬프트에 목적을 명시하는 규칙을 Bash/PowerShell 한정에서
  "권한을 요구하는 모든 도구 호출"로 범위를 넓혀 저장함(`feedback_bash-description-purpose.md`).
  WebSearch 도구는 `description` 파라미터 자체가 없어 이 규칙을 못 지킨다는 것도 확인 —
  그런 도구는 호출 전 텍스트로 목적을 먼저 밝히는 방식으로 보완하기로 함.
- Wayfinder 맵(`.scratch/ime-focus-accuracy/map.md`)은 destination에 도달해 완료됐지만,
  "Not yet specified"에 `TF_IPPMF_ENABLEPROFILE` 실패 시 무엇을 상의할지는 아직 fog로
  남아있다(실험 결과가 나와야 구체화 가능) — 다음에 이 맵을 다시 열어볼 상황이 생기면
  참고할 것.
