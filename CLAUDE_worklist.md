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

## 3. TSF 연동 (핵심 로직)

- [x] **TSF COM interop 선언**
  - GUID/vtable을 손으로 옮기다 실수하면 크래시하거나 조용히 잘못된 메서드가 호출될
    위험이 커서, 직접 작성하는 대신 검증된 `Vanara.PInvoke.TextServicesFramework`
    NuGet 패키지의 `ITfThreadMgr`/`ITfThreadMgrEventSink`/`ITfCompartmentMgr`/
    `ITfCompartment`/`ITfSource`/`ITfCompartmentEventSink`/`GUID_COMPARTMENT_KEYBOARD_OPENCLOSE`
    선언을 그대로 사용한다.
  - 검증방법: 컴파일이 되는지, 그리고 다음 항목(스레드 매니저 초기화)에서 실제로 호출해
    동작하는지로 간접 확인. TDD 대상 아님 — 라이브러리 선언을 그대로 참조하는 것이라
    우리 쪽에 검증할 동작이 없음. (CLAUDE.md 4절)
- [ ] **스레드 매니저 초기화 + 최초 상태 조회**
  - 검증방법: 실제 앱을 띄워 시작 시 초기 화면이 현재 한/영 상태와 일치하는지 수동으로
    확인. TDD 대상 아님 — 실제 `ITfThreadMgr` COM 객체 생성/조회라 글루 코드. 다만 이 COM
    호출을 인터페이스(예: `ICompartmentReader`) 뒤로 감싸 두면, 아래 두 항목에서 그 인터페이스의
    가짜 구현을 이용한 TDD가 가능해지므로 여기서 그 경계를 만들어 둔다. (CLAUDE.md 4절)
- [ ] **포커스 전환 감지 및 컴파트먼트 구독 전환**
  - 검증방법: 여러 앱 사이를 오가며 포커스를 바꿔보고, 각 앱의 한/영 상태가 독립적으로
    반영되는지 수동으로 확인. TDD 대상 아님 — `OnSetFocus`에서 `UnadviseSink`/`AdviseSink`를
    호출하는 것 자체가 실제 COM 상태 변경이라 글루 코드. (CLAUDE.md 4절, ADR-0002)
- [ ] **컴파트먼트 변경 이벤트 연결**
  - 검증방법: "새로 읽은 원시값 또는 조회 실패 → 다음에 표시할 상태" 판단 로직(컴파트먼트
    없음/COM 실패 시 마지막 상태 유지, 최초엔 영문 기본값)은 COM과 분리된 순수 상태 전이
    함수로 뽑아 TDD로 먼저 작성한다. 그 함수를 실제 `OnChange` 콜백에 연결하는 부분만
    실행해서 수동으로 확인. (CLAUDE.md 4절)

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

- CLAUDE.md 재그릴링: 인디케이터 크기/여백을 실사용 피드백에 따라 36px/5px →
  26px/2px로 재조정, DPI 배율 대응 설계 결정.
- 멀티 모니터 배치 + DPI 스케일링 구현, 100%/125% 혼합 배율 환경에서 실측 검증
  완료 (`feature/ime-indicator-app` 브랜치, 커밋 `0f0dcad`).
- 클릭 반전 + 전체 동기화 구현 (`053ef5b`).
- `Vanara.PInvoke.TextServicesFramework` 패키지 도입 (`4c49e4f`).
- TSF 스레드 매니저 초기화 + `ITfThreadMgrEventSink`/`ITfCompartmentEventSink`
  구독을 C#으로 구현 (`2b5e9da`, 디버그 로그 포함) — 하지만 실측 결과 **다른
  프로세스의 포커스/컴파트먼트 변경을 전혀 감지하지 못하는 것으로 확인**됨.
- 원인 조사 결과, 독립 EXE의 자체 `ITfThreadMgr`로는 구조적으로 다른 프로세스의
  TSF 상태를 볼 수 없다는 결론에 도달 → TIP(Text Input Processor) 등록 방향으로
  ADR-0003 작성, 새 브랜치 `feature/ime-state-detection`으로 이어감.
- C++ 프로토타입(`prototype/tip-detection-poc-throwaway` 브랜치, push 완료)으로
  TIP 방식을 실측 검증:
  - ✅ **TIP은 실제로 다른 프로세스에 로드된다** — 등록만 해두면 사용자가 키보드로
    선택하지 않아도 텍스트 입력을 다루는 거의 모든 프로세스에 로드되고 `Activate()`가
    호출됨. ADR-0003의 핵심 전제는 맞았다.
  - ❌ **하지만 `GUID_COMPARTMENT_KEYBOARD_OPENCLOSE`는 전혀 관찰되지 않는다** —
    문서/전역/컨텍스트 세 스코프 전부, 그리고 컨텍스트에 실제로 존재하는 모든
    컴파트먼트(4~5개)를 나열해 전부 구독해봐도, 메모장·탐색기 양쪽에서 실제
    한/영 전환(완성된 한글 입력 확인됨)을 했는데도 값이 전혀 안 바뀌고 `OnChange`도
    한 번도 안 옴.
- ADR-0003을 `superseded` 표시, ADR-0004에 프로토타입 결과와 다음 방향 후보 기록.
- PoC가 남긴 레지스트리(COM/TSF 등록) 흔적은 전부 제거 확인함(5개 CLSID 모두
  조회 시 "찾을 수 없음"). 파일도 저장소에서 삭제, 소스만 throwaway 브랜치에 보존.

### 미완료 상태로 남은 작업과 현재 상태

- `CLAUDE_worklist.md`의 "3. TSF 연동" 섹션 — **"스레드 매니저 초기화 + 최초 상태
  조회"부터 그 아래 항목들 전부 아키텍처 재검토 대기 상태.** 지금 `feature/ime-indicator-app`
  브랜치에 있는 C# TSF 연동 코드(`TsfImeStateMonitor` 등)는 동작하지 않는 것으로
  확인됐으므로, 체크박스는 아직 미완료(`[ ]`)로 둔다 — 새 방향이 정해지면 이
  워크리스트 자체를 다시 써야 할 가능성이 높다.
- 실제 Microsoft 한국어 IME가 어떤 메커니즘으로 자신의 열림/닫힘 상태를 노출하는지
  (혹은 노출하지 않는지) 아직 못 찾음.

### 다음에 시작할 지점

1. `feature/ime-state-detection` 브랜치에서 이어서 시작.
2. `docs/adr/0004-tip-prototype-inconclusive.md`의 "다음으로 검토할 만한 방향"
   섹션부터 — 후보는 (a) 우리 TIP을 실제 활성 입력기로 전환해서 재검증(범위가
   커짐), (b) Microsoft 한국어 IME가 실제로 쓰는 메커니즘 추가 조사, (c) TSF/TIP
   경로 자체를 재검토(애초 동기였던 AHK의 부정확함과 다시 비교).
3. 방향이 명확치 않으므로 `/grill-with-docs`로 다시 그릴링해서 확정 권장.

### 특이사항 / 참고

- `git push`는 사용자 요청으로 보류 중 — `feature/ime-state-detection`은 원격보다
  1커밋 앞서 있음(ADR-0003/0004 커밋). `prototype/tip-detection-poc-throwaway`는
  이미 push 완료.
- PoC 코드/실측 로그 원본은 `prototype/tip-detection-poc-throwaway` 브랜치의
  `prototype/tip-detection-poc/`에 그대로 남아 있음 (버전별 실험 과정 포함,
  `README.md` 참고).
- 커밋 메시지는 스킬 이름 언급 없이 실제 변경 내용 중심으로, 대화는 한글로 —
  기존 메모리 규칙 계속 적용 중.
