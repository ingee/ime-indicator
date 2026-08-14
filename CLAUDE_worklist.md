# CLAUDE_worklist.md — 구현 작업 계획 (보관용)

> **보관용 문서 — 더 이상 갱신하지 않는다.** 2026-08-14 세션에서 TIP `Activate()` 미호출
> 회귀와 아키텍처 재검토가 시작되면서, 현재 진행 상황과 다음 시도는
> `.scratch/ime-detection-strategy/map.md` wayfinder 맵을 따른다. 이 파일은 그 이전까지
> 실제로 구현·검증했던 내용의 기록으로만 남겨둔다.

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
    "테스트할 동작" 자체가 없음. (CLAUDE.md 4절, docs/ui-spec.md)
- [x] **설정값(Constants) 모듈**
  - 검증방법: 코드 리뷰(값이 docs/ui-spec.md와 일치하는지 눈으로 대조)로 충분. TDD 대상
    아님 — 상수 나열이라 동작이 없음. (docs/ui-spec.md)

## 2. 인디케이터 UI (TSF 연동 이전 — 하드코딩 상태로 렌더링 검증)

- [x] **인디케이터 창 구현**
  - 검증방법: "상태(한글/영문) → 배경색/텍스트" 매핑 함수는 TDD로 진행 — 테스트를 먼저
    작성해 빨강/"한", 파랑/"A" 매핑을 검증한다. 반면 `CreateParams` 오버라이드로 만드는
    topmost·no-activate·tool window 속성은 실제 창을 띄워 눈으로 확인해야 하는 글루 코드라
    TDD 대상이 아니며, 앱을 실행해 수동으로 확인한다. (docs/ui-spec.md)
- [x] **멀티 모니터 배치**
  - 검증방법: "모니터 경계 + 인디케이터 크기 + 상단 여백 → 창 위치(x, y)" 계산 함수는 TDD로
    진행 — 임의의 모니터 좌표를 입력으로 줘서 기대 좌표가 나오는지 테스트한다. `Screen.AllScreens`
    열거와 실제 폼 생성/배치는 글루 코드이므로 실제 모니터 환경에서 눈으로 확인한다.
    (docs/ui-spec.md)
- [x] **클릭 반전 + 전체 동기화**
  - 검증방법: "현재 표시 상태 → 반전된 상태" 전이와 "모든 모니터에 같은 값 전파"는 UI 이벤트와
    분리된 상태 관리 클래스/함수로 뽑아 TDD로 진행 가능 — 클릭 이벤트 자체는 그 로직을
    호출하는 얇은 연결부라 실행해서 수동으로 클릭해보는 정도로 확인한다. (docs/ui-spec.md)

## 3. TIP + IPC 연동 (ADR-0005)

- [x] **TIP DLL 뼈대 + 등록/로딩 검증**
  - `src/ImeIndicatorTip/` 프로젝트 생성(vcxproj), `DllRegisterServer`/`DllUnregisterServer`/
    `DllGetClassObject`/`DllCanUnloadNow` + 빈 `Activate`/`Deactivate`만 구현.
  - 검증방법: `regsvr32`로 등록 후 메모장을 완전히 종료했다가 새로 띄워 `Activate()`가
    호출되는지(임시 로그로) 확인 완료. **HKCU\Software\Classes만으로는 COM 등록은 성공해도
    TSF가 실제로 로드하지 않고, HKLM에 등록해야 로드된다는 것을 실측으로 확인** — 관리자
    권한이 필요하다는 뜻(ADR-0005 Update, docs/ui-spec.md 반영 완료). `regsvr32 /u`로 등록
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
    아님 — 에셋 제작과 `NotifyIcon` 연결이라 동작이라 부를 게 없음. (docs/ui-spec.md)
- [ ] **시작프로그램 등록/해제**
  - 검증방법: "현재 등록 여부(bool) → 메뉴 라벨/체크 상태" 판단 로직은 TDD로 먼저 작성.
    실제 레지스트리 읽기/쓰기는 인터페이스로 감싸 가짜 구현으로 등록/해제 흐름까지 TDD로
    검증할 수 있다. 다만 실제 레지스트리 키가 제대로 생기고 로그인 시 실행되는지는 마지막에
    한 번 수동으로 확인한다. (docs/ui-spec.md)

## 5. 마무리

- [ ] **방어적 처리 보강 및 전체 시나리오 점검**
  - 검증방법: 여러 앱을 오가며 실제 한/영 전환, 클릭 교정, 포커스 전환, (가능하면) COM 호출
    실패 상황까지 수동으로 시나리오를 돌려 크래시 없이 인디케이터가 유지되는지 확인. TDD
    대상 아님 — 이 항목 자체가 수동 검증 단계. (CLAUDE.md 4절)
- [ ] **Self-contained 단일 파일 게시 검증**
  - 검증방법: `dotnet publish -r win-x64 --self-contained -p:PublishSingleFile=true` 실행 후
    결과 exe를 별도 .NET 런타임 없는 환경(또는 그렇다고 가정하고)에서 실행해 정상 동작하는지
    확인. TDD 대상 아님 — 빌드/배포 절차 검증. (docs/ui-spec.md)
