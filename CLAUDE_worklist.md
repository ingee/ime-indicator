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

## 마지막 세션 요약 (2026-08-09)

### 오늘 완료한 작업

`Activate()`가 Notepad/Notepad++ 어디에서도 호출되지 않는 핵심 회귀를 계속 조사한 하루.
결론(완전 해결)까지는 못 갔지만, 유력한 가설 하나를 기각하고 새 가설 하나를 좁혔다.

- **재부팅 → 로그인 직후 타이밍 가설 검증**: 재부팅(11:57:31) 후 새로 뜬 패키지형 Notepad에서
  재현 테스트 — 회귀 계속 재현. `ImeStateTip.cpp`에 파일 기반 진단 로그(`DiagLog`, 경로
  `.scratch/ime_tip_debug.log`)를 `Activate`/`SubscribeThreadScopeCompartment`/
  `ReportCurrentValue`/`OnChange`에 추가.
- **모듈 로드 자체가 안 되는 것을 재확인**: `Get-Process -Id <pid> | Modules` 스캔 결과
  Notepad/Notepad++ 어디에도 `ImeIndicatorTip.dll` 미로드. `CoCreateInstance`로 독립 호출하면
  성공 — DLL/COM 등록 자체는 멀쩡하고 "TSF가 로드하기로 선택하지 않는" 지점 문제로 좁혀짐.
- 스탠드얼론 진단 도구 `_diag_check_enabled.cpp`/`_build_diag.bat` 작성 —
  `ITfInputProcessorProfileMgr::GetProfile`/`EnumProfiles`로 우리 프로필의
  `ACTIVE`/`ENABLED` 플래그를 직접 조회.
- 다음 가설을 순서대로 테스트해 **전부 배제**: "이전 버전 Microsoft IME" 토글 OFF(효과 없음),
  `HKLM\...\CTF\TIP\{CLSID}` 레지스트리 패턴(정상), `DeactivateProfile`로 `ENABLED=0` 되돌리기
  (여전히 미로드), AppLocker/CodeIntegrity/Defender/Smart App Control(차단 흔적 없음),
  `ctfmon.exe` 강제 재시작(효과 없음).
- 스크린샷 단서: 언어 전환 팝업에 `ingee.ImeIndicatorTip`이 Microsoft IME와 나란히 "선택
  가능한 독립 키보드"로 노출 — Settings의 "설치됨" 계층과 TSF의 "enabled 프로필" 계층이
  분리돼 있어 GUI로는 제거 불가.
- `/resume`로 세션 재개 후, **타이밍 가설을 실측으로 기각**: 재부팅 2분 37초 후 새로 시작한
  `mintty`와 7분 후 새로 시작한 Notepad++ 둘 다 DLL 미로드 확인(`Get-Process`로 모듈 직접
  스캔). 재부팅 직후냐 아니냐가 원인이 아님이 확인됨.
- **`ENABLED`≠`ACTIVE` 구분 발견**: `_diag_check_enabled.cpp`를 `DeactivateProfile` 대신
  `ActivateProfile(TF_IPPMF_ENABLEPROFILE)`을 호출하도록 수정·재빌드해 실행 →
  `ENABLED=1`은 재조회해도 유지되지만 `ACTIVE`는 그 COM 세션이 끝나면 다시 `0`으로 돌아옴.
  `ENABLED=1` 상태로 완전히 새 Notepad++ 프로세스(`-multiInst`)를 띄워 재테스트했으나
  **여전히 미로드** — `ENABLED=1`만으로는 부족하다는 뜻. `ACTIVE=1`(실제 선택된 키보드)이
  필요조건일 수 있다는 새 가설이 생겼으나, 이를 만들려면 `TF_IPPMF_FORPROCESS`/`FORSESSION`이
  필요해 CLAUDE.md 3절 비목표("표시 전용")와 충돌하고, 8/7~8/8엔 `ACTIVE=0`인 채로도 분명히
  동작했던 사실과도 모순돼 **아직 완전히 설명되지 않은 채 남음**.
- 다음 조사 방향(새 CLSID 재등록 vs Process Monitor 설치 vs 잠정 보류)을 사용자에게
  물으려던 중, 사용자가 대신 **전체 정리**를 요청 — 아래 "완료한 정리 작업" 참고.
- **TIP DLL 레지스트리 완전 정리**: `regsvr32 /u`로 `DllUnregisterServer` 호출 →
  `HKLM\SOFTWARE\Classes\CLSID\{8CD02B2A-...}`, `HKLM\SOFTWARE\Microsoft\CTF\TIP\{8CD02B2A-...}`,
  `HKCU` 쪽 모두 제거 확인. `Get-WinUserLanguageList`의 `ko` 키보드 목록에서
  `ingee.ImeIndicatorTip`이 사라지고 Microsoft IME만 남음 확인. 시스템 전체 프로세스 스캔으로
  DLL이 로드된 프로세스가 없음도 확인. `ctfmon.exe` 재시작으로 언어 전환 팝업 UI 캐시도 갱신.
  **현재 TIP은 완전히 미등록 상태.**
- `README.md` 개편(사용자 요청): "알려진 문제" 섹션(TIP+IPC 이전 아키텍처를 설명하던 낡은
  내용) 제거, `CLAUDE.md`/`CLAUDE_worklist.md` 참조 없이 README만으로 완결되도록 재작성,
  "개발/검증 환경" 섹션 신설(Windows 11 Pro·TSF 전용·"이전 버전 Microsoft IME" 옵션 켜짐 명시).
- **워크플로우 컨벤션 확정**: `/resume`가 "마지막 세션 요약"을 소비한 뒤 그 섹션을 삭제하는
  것이 사용자의 의도된 컨벤션임을 확인(이번 세션 중간에 이 섹션이 사라진 걸 발견하고 데이터
  유실로 오인해 `git diff`/`reflog`/CRLF까지 조사했으나 실제로는 사용자가 의도적으로 삭제한
  것이었음). 전역 스킬 `~/.claude/skills/resume/SKILL.md`(소비 후 삭제 단계 추가)와
  `~/.claude/skills/wrap-up/SKILL.md`(섹션이 없는 게 정상 상태라는 안내 추가)에 이 컨벤션을
  반영해 모든 프로젝트에 일관 적용되도록 함.

### 미완료 상태로 남은 작업과 현재 상태

- **핵심 회귀 여전히 미해결.** `ENABLED`만으로는 로드되지 않는다는 것까지는 확인했지만,
  정확히 무엇이 있어야 로드되는지는 아직 모른다. 과거(8/7~8/8) 정상 동작 시점의
  `ENABLED`/`ACTIVE` 값을 확인 못 한 채 지나간 게 뼈아프다 — 그때는 이 진단 도구 자체가
  없었다.
- **TIP은 현재 완전히 미등록 상태**(사용자 요청으로 정리 완료) — 다음 세션에서 조사를
  재개하려면 재등록부터 해야 한다(`regsvr32`, 관리자 권한 필요, ADR-0005 Update 참고).
- Excel/Word/PowerPoint 저위험 실험(ADR-0006)과 `cmd.exe`용 `OnSetFocus` 구독(워크리스트
  3절 나머지 항목들)은 이 핵심 회귀가 먼저 해결되기 전까지 계속 보류.
- 다음 조사 방향에 대한 사용자 결정이 아직 없음(새 CLSID 재등록 / Process Monitor 설치 /
  다른 방안) — 다음 세션에서 다시 상의해야 한다.

### 다음에 시작할 지점

1. **다음 조사 방향부터 사용자와 다시 상의할 것** — 이번 세션에서 결정 못 하고 넘어감. 후보:
   (a) 완전히 새 CLSID/프로필 GUID로 재등록해서 기존 CLSID에 묶인 캐시/상태 문제인지 구분
   (추천 — 기존 CLSID는 지난 세션들의 여러 실험으로 상태가 오염됐을 가능성이 있음),
   (b) Sysinternals Process Monitor 설치해서 Notepad/Notepad++ 시작 시 TSF 관련 레지스트리
   조회 과정을 직접 추적(현재 미설치), (c) `ACTIVE=1`이 정말 필요조건인지 확인하되
   `TF_IPPMF_FORPROCESS`/`FORSESSION`은 CLAUDE.md 비목표와 충돌하므로 신중히 재검토.
2. 재등록부터 해야 재조사가 가능하다 — `regsvr32 "C:\_data\git\ime-indicator\src\ImeIndicatorTip\x64\Debug\ImeIndicatorTip.dll"`(관리자 PowerShell). CLSID는
   `{8CD02B2A-A5A2-4902-A9F7-8ECFCCCF89E2}`, 언어 프로필 GUID는
   `{986562BD-97A1-4877-A1FD-082713973899}`(`Registration.cpp`/`Guids.h` 참고).
3. `src/ImeIndicatorTip/_diag_check_enabled.cpp`는 현재 마지막 동작이 `DeactivateProfile`이
   아니라 `ActivateProfile(TF_IPPMF_ENABLEPROFILE)`로 바뀐 상태다 — 재실행할 때마다 자동으로
   `ENABLED=1`이 켜진다는 점 유의(전에는 반대로 매번 꺼졌었음). 재사용 시 이 사실을 먼저
   상기할 것.
4. `ImeStateTip.cpp`의 `DiagLog` 진단 코드는 아직 유지 중(미커밋) — 조사 계속할 거면 그대로
   두고, 로그 경로는 `.scratch/ime_tip_debug.log`.

### 특이사항 / 참고

- **"이전 버전의 Microsoft IME" 토글은 절대 끄라고 제안하지 말 것.** 사용자가 Vim 사용자라
  `C:\_data\git\AutoHotkey\for VIM ESC.ahk`(ESC로 한→영 강제 전환)를 상시 사용 중이고, 이
  토글이 꺼지면 그 스크립트가 부자연스럽게 동작한다. 이번 세션에서도 다시 한번 이 토글을
  끄면 워크플로우만 깨지고 버그 재현에는 영향이 없다는 게 재확인됐다.
- **미커밋 변경사항 현황**: `CLAUDE_worklist.md`(이 요약), `README.md`(개발/검증 환경 섹션),
  `src/ImeIndicatorTip/ImeStateTip.cpp`(diag 로그), `src/ImeIndicatorTip/_diag_check_enabled.cpp`
  (Activate로 수정), 신규 미추적 `_build_diag.bat`/`_diag_check_enabled.exe`. 전부 다음
  세션에서도 유용하게 재사용 가능 — 지우지 말 것.
- `Enable`/`ENABLED`·`ACTIVE` 플래그는 `HKLM\...\CTF\TIP\{CLSID}\LanguageProfile\...`의 정적
  레지스트리 값과 다른 런타임 계층이다 — 이번 세션에 `ITfInputProcessorProfileMgr` API로 직접
  조회하는 방법을 확립했으니 앞으로 이 값들을 볼 땐 레지스트리 대신 이 API를 우선 사용할 것.
- `x64/Debug/`에 지난 세션들의 DLL 파일 잠금 우회 잔여물(`.locked`, `.locked2` 등)이 계속
  쌓이고 있다 — 당장 급한 건 아니지만 언젠가 한 번 정리 필요.
