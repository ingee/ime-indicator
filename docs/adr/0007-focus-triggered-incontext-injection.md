# 포커스-트리거 WINEVENT_INCONTEXT 주입으로 TIP 자동 로드 의존을 제거한다

ADR-0003~0006이 세운 아키텍처는 전부 "Windows가 우리 TIP을 텍스트 입력을 다루는 프로세스마다
자동으로 로드하고 `ITfTextInputProcessor::Activate()`를 호출해준다"는 전제 위에 서 있었다. 이
전제는 2026-08-07까지는 실제로 성립해 정확·이벤트 기반으로 동작했지만, 2026-08-08 이후
원인 불명의 회귀로 대부분의 프로세스에서 깨졌다. 세 세션에 걸친 원인 조사(CLSID 오염,
`ENABLED`/`ACTIVE` 레지스트리 플래그, `ctfmon.exe` 재시작, 재부팅, 등록 코드 자체의 변경,
보안/VPN 소프트웨어 — `.scratch/ime-detection-strategy/issues/01`, `02`)를 거쳤지만 근본 원인을
찾지 못했다.

`.scratch/ime-detection-strategy/issues/07-focus-triggered-incontext-injection.md`에서, 그
자동 로드 진입점 자체에 대한 의존을 포기하고 대안을 실측했다. `prototype/langbar-observation-poc-throwaway`
브랜치의 `LangBarPoc10`/`LangBarPoc10-x86`/`LangBarPoc11`로 5단계를 검증:

1. `SetWinEventHook(EVENT_SYSTEM_FOREGROUND, ..., WINEVENT_INCONTEXT)`를 시스템 전역
   (`idProcess=0`)으로 걸면, 포커스가 바뀔 때마다 그 순간 포커스를 얻은 프로세스 안에 우리
   콜백 DLL이 실제로 로드된다 — 단, **훅 설치 프로세스와 대상 프로세스의 비트니스가 같을
   때만**. 비Office 프로세스(탐색기, Firefox, Notepad++, mintty 등) 20건 이상에서 인프로세스
   실행이 예외 없이 확인됐다.
2. 이 이벤트는 드롭 없이 신뢰성 있게 발생한다 — ADR 이전 조사(issue 04)에서 확인된
   `EVENT_OBJECT_NAMECHANGE`(10번 중 1번꼴)보다 훨씬 안정적이다.
3. 콜백이 실행되는 스레드는 대체로 그 창의 UI 스레드와 일치하지만, 예외가 1건 관측됐다(메모장) —
   프로덕션 구현 시 스레드 마샬링이 필요하다는 뜻으로, 아직 미구현이다.
4. **핵심 검증**: 콜백 안에서 TSF가 우리를 `Activate()`해주길 기다리지 않고, 우리가 직접
   `CoCreateInstance(CLSID_TF_ThreadMgr)` → `Activate()`를 호출해도 TSF는 이를 정상적인
   참가자로 받아들인다. 그 위에 ADR-0005가 확립한 "스레드 스코프 컴파트먼트"(`ITfThreadMgr`을
   `ITfCompartmentMgr`로 직접 QI, `GUID_COMPARTMENT_KEYBOARD_OPENCLOSE` 구독) 로직을 그대로
   올렸더니, Notepad++에서 한/영을 20회 전환하는 동안 `OnChange`가 누락 없이 정확한 값으로 전부
   잡혔다 — 8/7 이전 TIP이 냈던 결과와 동일하다.
5. x64 훅과 x86 훅을 동시에 띄우면 32비트 프로세스(이 PC의 Office 2010 = Excel/Word/PowerPoint)
   까지 커버된다. 이건 기존 TIP 방식이 **애초에 한 번도 달성하지 못했던** 범위다 —
   `src/ImeIndicatorTip`는 x64 전용으로만 빌드돼 왔고, 이는 8/8 회귀와 무관하게 처음부터 있던
   공백이었다(사용자 확인).

## 결정

포커스-트리거 `WINEVENT_INCONTEXT` 주입을 새로운 핵심 감지 메커니즘으로 채택한다. Windows의
자동 TIP `Activate()` 진입점(ADR-0003의 핵심 전제)에는 더 이상 의존하지 않는다 — issue 02
("왜 그 진입점이 고장났는가")는 더 이상 풀어야 할 이유가 없어져 `## Answer`로
"아키텍처 전환으로 무관해짐"을 남기고 resolved 처리한다.

## Considered Options

- **issue 02를 계속 파서 회귀 원인을 규명하고 기존 TIP 경로를 복구** — 세 세션을 투입해도
  근본 원인을 못 찾았고 남은 리드도 소진됐다. 이슈 07이 그 미해결 상태와 무관하게 동작하므로,
  포기는 아니지만 더 이상 우선순위를 둘 이유가 없다.
- **issue 04의 작업표시줄 관찰(폴링 기반)** — 공식 API(`ITfLangBarItemMgr`)의 크로스프로세스
  조회는 원천 불가능, UI Automation은 상태를 노출하지 않음, 이벤트 기반 관찰
  (`EVENT_OBJECT_NAMECHANGE`)은 소스 자체가 비일관적. 폴링 기반 화면 캡처는 정확도·비용 모두
  검증됐지만 CLAUDE.md 3절의 "폴링 금지" 원칙과 정면 충돌한다. 이슈 07이 이벤트 기반으로
  성공했으므로 채택할 필요가 없어졌다 — 문서상 "최후의 안전망"으로만 남긴다.

## Consequences

- **재사용되는 것**: ADR-0005의 스레드 스코프 컴파트먼트 발견(핵심 QI 패턴)은 그대로 유효하며
  새 진입점 안에서 변경 없이 재사용된다. ADR-0002의 포커스별(창 안 컨트롤 전환) 재구독 로직도
  새 진입점 위에 그대로 얹을 계획이지만, 아직 실측하지 않았다 — 다음 작업 항목.
- **더 이상 필요 없어지는 것**: ADR-0006이 권고한 Excel `TF_IPPMF_ENABLEPROFILE` 저위험
  실험은 불필요해졌다 — Office의 자동 `Activate()`가 되든 안 되든 우리가 직접 `ThreadMgr`를
  만들기 때문이다. ADR-0005/0006은 이 ADR로 대체된 것으로 표시한다(`status: superseded by
  ADR-0007`) — 다만 두 문서가 남긴 실측 사실(스레드 스코프 QI 패턴, PID 매칭 예외 사례)은
  계속 유효한 기록으로 남는다.
- **남은 위험**(issue 07 결론 참고, 전부 구현 착수 전 항목): 스레드 불일치 시 마샬링
  미구현, 크래시 블라스트 반경(포커스 전환마다 임의 프로세스에 실제 로드됨), AV/EDR 오탐 소지,
  포커스별 재구독 로직 미검증.
- **배포 방식이 다시 바뀐다**: 기존 TIP은 COM 등록(관리자 권한 1회)만 필요했지만, 이 방식은
  x64+x86 훅 프로세스가 상시 실행되며 시스템 전역 WinEvent 훅을 유지해야 한다.
  `docs/ui-spec.md` 갱신은 후속 작업으로 남긴다.
- 프로토타입 코드는 `prototype/langbar-observation-poc-throwaway` 브랜치의 `LangBarPoc10`
  (x64)/`LangBarPoc10-x86`/`LangBarPoc11`(TSF 연결)에 보존한다.
- `src/ImeIndicatorTip`(기존 TIP DLL)는 당장 삭제하지 않는다 — 새 구현이 이를 완전히
  대체할 때까지 참고용으로 남긴다.

## Update — 프로덕션 구현 및 실사용 검증 (2026-08-15)

프로토타입을 `src/ImeFocusHook/`(`ImeFocusHookDll.vcxproj` x64+Win32, `ImeFocusHookLoader.vcxproj`
x64+Win32)로 옮겨 실제 구현했다. `src/ImeIndicator`(UI/IPC 쪽)는 계획대로 한 줄도 안 바뀌었다 —
기존 `ImeStateIpcListener`/`ForegroundWindowTracker`/`ForegroundStateResolver`가 이미 이 새
아키텍처가 필요로 하는 모델(PID 키, 마지막 상태 유지)을 구현하고 있었기 때문이다.

프로토타입 대비 프로덕션에서 추가/변경한 것: 프로세스 전역 구독 플래그를 `thread_local`로
바꿔 같은 프로세스의 다른 UI 스레드도 독립적으로 구독되게 함, 파일 로깅을 실제 IPC 리포트로
교체, 로더 EXE에 이름 붙은 뮤텍스(`ingee.ImeIndicator.FocusHookLoader.x64`/`.x86`) 기반 단일
인스턴스 가드 추가(재실행 시 훅 중복 등록 방지 — 중복 실행 시 두 번째 인스턴스가 즉시 종료됨을
실측 확인), DLL 경로를 절대경로 대신 자기 자신의 모듈 경로 기준 상대 참조로 변경.

사용자가 메모장을 포함한 여러 앱과 **Excel**을 오가며 한/영 전환을 직접 검증 — 전부 정확히
반영됨을 확인("완벽해"). 프로토타입 단계에서만 확인됐던 것이 실제 프로덕션 코드 경로로도
재현됨을 확정. 이번 구현 범위 밖으로 남긴 항목(스레드 마샬링, `cmd.exe`/`conhost.exe` 재검증,
크래시 자동 재시작, `docs/ui-spec.md` 갱신, `src/ImeIndicatorTip` 정리)은 여전히 미착수 —
`.scratch/ime-detection-strategy/map.md`의 "Not yet specified" 참고.
