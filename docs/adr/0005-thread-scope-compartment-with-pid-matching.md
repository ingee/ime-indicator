# TIP의 스레드 스코프 컴파트먼트 + PID 매칭으로 IME 상태 감지

ADR-0004는 `GUID_COMPARTMENT_KEYBOARD_OPENCLOSE`를 문서/전역(`GetGlobalCompartment`)/컨텍스트
세 스코프에서 다 구독해봐도 값이 관찰되지 않는다는 결론을 남기고, 다음 방향 중 하나로
"TSF/TIP 경로 자체를 재검토"를 포함해 여러 후보를 나열했다. 이 중 실제로 시도하지 않은
네 번째 스코프가 있었다 — `ITfThreadMgr::GetGlobalCompartment()`를 거치지 않고
`ITfThreadMgr` 자신을 `ITfCompartmentMgr`로 직접 `QueryInterface`해서 얻는 **스레드 스코프**
컴파트먼트다. "전역"이라는 이름과 달리 이건 그 스레드매니저 인스턴스 전용 저장소라
데스크톱 전체가 공유하는 객체가 아니다.

`prototype/tip-detection-poc-throwaway` 브랜치의 `TipPoc6`로 이 스코프를 검증했다:
메모장에서 실제로 한/영을 반복 전환하며 입력하자, 이 스코프의 `GUID_COMPARTMENT_KEYBOARD_OPENCLOSE`
값이 `0`/`1`로 정확히 토글됐고 `OnChange`도 매번 정상적으로 왔다. 같은 순간 문서/전역/컨텍스트
세 스코프는 여전히 비어 있었다(`VT_EMPTY`). 다른 프로세스에 로드된 TIP 인스턴스는 이 변화에
전혀 반응하지 않아, "앱마다 입력 상태가 독립적으로 유지된다"는 기존 확인(ADR-0002)과도
들어맞았다.

다만 이 값은 "그 TIP 인스턴스가 활성화된 스레드"에만 스코프돼 있어, UI 프로세스가 이를
쓰려면 "지금 OS 포커스를 가진 앱이 어느 프로세스인지"와 매칭해야 한다. `TipPoc7`로 추가
검증한 결과, **`GetForegroundWindow()` 기준 프로세스ID는 TIP이 활성화된 프로세스ID와 항상
일치했지만, 스레드ID는 앱마다 달랐다** — 클래식 Win32 단일 스레드 앱(예: mintty)은
일치했지만, Windows 11의 패키지형(WinUI) 메모장처럼 TIP이 활성화되는 스레드와 실제 창을
소유하는 스레드가 분리된 앱에서는 불일치했다. 따라서 매칭 키는 **PID**로 정한다.

## Considered Options

- ADR-0004가 나열한 다른 방향들(우리 TIP을 실제 활성 입력기로 전환, IME 내부 리버스
  엔지니어링, TSF/TIP 경로 자체 재검토)은 이번 발견으로 불필요해졌다 — "곁에서 관찰만
  하는 최소 TIP"이라는 ADR-0003의 원래 전략이 스코프만 수정하면 그대로 성립한다.
- 스레드ID를 매칭 키로 쓰는 방안은 TipPoc7에서 최신 패키지형 앱 기준으로 기각됐다.

## Consequences

- ADR-0004는 이 ADR로 대체된 것으로 표시한다(`status: superseded by ADR-0005`) — 다만
  ADR-0004가 기록한 "문서/전역/컨텍스트 스코프는 관찰 안 됨"이라는 사실 자체는 계속
  유효한 실측 기록으로 남긴다.
- TIP DLL의 책임은 ADR-0003의 원안대로 유지한다: 자신이 로드된 프로세스의 스레드 스코프
  `GUID_COMPARTMENT_KEYBOARD_OPENCLOSE`를 구독해 상태가 바뀔 때마다 자신의 PID와 함께
  UI 프로세스에 보고한다.
- UI 프로세스는 TIP과 별개로 OS 포그라운드 전환을 추적해야 한다(`SetWinEventHook`
  `EVENT_SYSTEM_FOREGROUND` 등 — TSF와 무관한 표준 Win32 API라 새 조사 없이 바로 설계
  가능). 포그라운드 창의 PID를 얻어, 그 PID의 TIP 인스턴스가 마지막으로 보고한 값을
  표시한다.
- TIP DLL(여러 프로세스에 로드)과 단일 UI EXE 사이의 IPC 방식(named pipe/공유 메모리 등)은
  아직 미결정이며 다음 설계 과제로 남는다.
- ADR-0003이 남긴 배포 방식 변경(관리자 권한 1회 설치 필요)도 여전히 유효하며, ADR-0001과
  CLAUDE.md 7절을 이 시점에 맞춰 갱신해야 한다.
- 프로토타입 코드(`TipPoc6`, `TipPoc7`)는 `prototype/tip-detection-poc-throwaway` 브랜치에
  보존한다.

## Update — 관리자 권한 필요 여부 실측 확인

`src/ImeIndicatorTip/` 뼈대 구현 중 CLSID를 `HKEY_CURRENT_USER\Software\Classes`에만
등록하고 메모장을 전부 종료했다가 완전히 새로 띄워 테스트한 결과, `Activate()`가 전혀
호출되지 않았다. 같은 코드를 `HKEY_LOCAL_MACHINE`으로만 바꿔 등록하면 새로 띄운 메모장에서
곧바로 `Activate()`가 호출됐다. `ITfInputProcessorProfiles::AddLanguageProfile`이 만드는
TIP 프로필(`HKLM\SOFTWARE\Microsoft\CTF\TIP\...`)도 CLSID 위치와 무관하게 항상 HKLM에
생긴다. 즉 **TSF의 TIP 로딩 판단은 일반 COM의 HKCU/HKLM 병합 조회를 따르지 않고 HKLM만
본다** — CLSID를 HKCU에만 등록해도 `CoCreateInstance`는 성공하지만 TSF는 그 TIP을 다른
프로세스에 로드하지 않는다.

**결론: TIP DLL 설치(`DllRegisterServer`)는 관리자 권한이 필요하다.** ADR-0003의 배포 방식
변경 예상이 그대로 확정됐다 — CLAUDE.md 7절, ADR-0001을 이 사실에 맞춰 갱신한다(별도
후속 결정으로 미뤄뒀던 부분).

## Update — 엔드투엔드 테스트로 확인된 두 가지 알려진 제약

전체 파이프라인(TIP → IPC → UI)을 실제로 붙여 여러 앱을 오가며 검증한 결과, 대부분의
GUI 텍스트 입력(메모장, Notepad++, mintty, Windows 11 패키지형 메모장 등 — 새로 뜬
프로세스 기준)은 정확히 반영됐다. 다만 두 가지 예외를 확인했다:

1. **클래식 콘솔 창(`cmd.exe` 등)** — 보이는 창(`GetForegroundWindow`)의 PID는 `cmd.exe`
   자신이지만, 실제 TSF/TIP이 로드되고 정확한 값을 보고하는 프로세스는 별도의
   `conhost.exe`다. 둘의 PID가 달라 UI 쪽 PID 매칭이 구조적으로 실패한다. Windows
   Terminal이나 mintty처럼 자체 창을 그리는 터미널에는 해당하지 않고, `conhost.exe` 기반
   클래식 콘솔에만 해당한다.
2. **Excel(및 추정컨대 다른 Office 앱)** — 셀 편집 모드에서 실제로 입력해도 TIP의
   `Activate()`조차 호출되지 않는다. `ITfCategoryMgr::RegisterCategory`로
   `GUID_TFCAT_TIP_KEYBOARD` 카테고리를 등록해도(다른 앱은 이 카테고리 없이도 로드했음)
   결과가 같아, 단순 등록 누락이 아니라 Office의 셀 그리드가 TSF와 다른 방식(레거시
   IMM 호환 경로 등)으로 텍스트 입력을 처리하는 것으로 추정된다 — 원인을 완전히 규명하지는
   못했다.

두 경우 모두 인디케이터는 크래시 없이 "마지막으로 알려진 상태를 유지"하는 CLAUDE.md 4절의
방어 규칙대로 동작한다(즉 안전하게 낡은 값을 보여줄 뿐, 잘못된 크래시나 예외는 없음). 다만
그 값이 실제 상태와 다를 수 있다는 게 이 두 경우의 한계다. 더 깊은 조사(예: Office의 텍스트
서비스 통합 방식 리버스 엔지니어링, `conhost.exe` PID를 추가로 추적하는 로직)는 투자 대비
효과가 불확실해 지금은 범위 밖으로 남기고, 알려진 제약으로 문서화한다.
