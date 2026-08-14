Type: research
Status: resolved

## Question

TIP을 시스템 전역에 심는 대신, Windows 자체가 이미 정확하게 그려주는 언어 표시줄/작업표시줄
IME 인디케이터를 관찰해서 상태를 얻는 아키텍처가 가능한가?

### 배경

- Windows 자체 입력 표시기는 Excel 셀 편집 중에도, `cmd.exe`에서도 정확하게 한/영 상태를
  반영한다는 게 이미 여러 세션에 걸쳐 확인됐다(2026-08-08 리서치, `docs/research/office-tip-activation-gap.md`;
  2026-08-14 조사). 즉 Windows 내부에는 이미 모든 프로세스의 정확한 상태를 아는 무언가가
  존재한다 — 우리가 그 정보를 직접 만들어내는 대신 "읽기만" 할 수 있다면, 지금까지의 TIP
  회귀 전체를 우회할 수 있다.
- 다만 그 정확한 정보가 "그려지는 UI"로만 노출되는지, 아니면 프로그램적으로 조회 가능한
  API/COM 인터페이스가 있는지는 아직 조사한 적이 없다.
- 이 방향이 성립하면 TIP DLL 전역 등록·관리자 권한 설치·회귀 디버깅 전체가 불필요해질 수
  있어 우선순위가 높다. 다만 완전히 새로운 아키텍처라 리서치 분량이 크다.

### 조사할 것

1. Windows 언어 표시줄(Language Bar)/작업표시줄 IME 인디케이터가 실제로 어떤 메커니즘으로
   상태를 얻는지 — TSF의 `ITfLangBarItemMgr`/`ITfLangBarItem` 계열 공식 API로 프로그램적
   조회가 가능한지 확인.
2. UI Automation(`IUIAutomation`)으로 작업표시줄의 IME 인디케이터 컨트롤을 찾아 그 텍스트/상태를
   읽어낼 수 있는지 — 다른 개발자들이 "현재 IME 상태를 코드로 읽는" 목적으로 이 방법을 쓴 사례가
   있는지 조사(예: AutoHotkey 커뮤니티, PowerToys 등 오픈소스).
3. `ITfLangBarItemSink`/`ITfLangBarEventSink` 같은 이벤트 구독 인터페이스가 있다면, 이게
   폴링 없이 상태 변화를 통지받을 수 있는 콜백 기반인지 확인(CLAUDE.md의 "폴링 금지" 원칙과
   부합해야 함).
4. 이 방식이 앱(창)별 독립 상태(ADR-0002)를 그대로 반영하는지, 아니면 시스템 전역의 단일
   값만 주는지 — 후자라면 지금 프로젝트가 요구하는 "포커스된 컨텍스트의 상태"와 다를 수 있어
   추가 검증 필요.

### 결론에 포함할 것

실현 가능성 판정(가능/불가능/조건부) + 가능하다면 필요한 API/권한 + TIP 방식과의 장단점 비교.

## Answer

**판정: 조건부 가능 — 단, "이벤트로 안다"는 조건은 실패했고 "폴링으로 읽으면 정확하다"는
조건만 성립한다.** 2026-08-15, `prototype/langbar-observation-poc-throwaway` 브랜치에서
`LangBarPoc1`~`LangBarPoc9` 9개 실측 프로토타입으로 검증했다(코드는 그 브랜치에만 있고
`main`/`feature`에는 없음 — throwaway 컨벤션).

**1번(공식 TSF API) — 불가능.** `ITfLangBarMgr::GetThreadLangBarItemMgr(dwThreadId, ...)`는
이름과 달리 크로스 프로세스 조회가 안 된다. 자기 자신의 스레드에서는 성공(`hr=0`)하지만 다른
프로세스의 포커스된 스레드에 대해서는 예외 없이 `E_FAIL`(`LangBarPoc1`, `ITfThreadMgr::Activate()`로
스레드를 TSF에 활성화해도 결과 동일).

**2번(UI Automation) — 상태 자체는 노출 안 함, 그러나 좌표 조회에는 유용.** 작업표시줄에서
"트레이 입력 표시기 한/영 전환" 버튼을 정확히 찾을 수 있었지만(`LangBarPoc2`), `Name`도
`LegacyIAccessible`의 `Value`/`Description`/`State`/`Role`도 전환 여부와 무관하게 고정값이었다
(`LangBarPoc3`, 17회 폴링 동안 전환해도 무변화). 다만 이 과정에서 얻은 **아이콘의 정확한 화면
좌표(`get_CurrentBoundingRectangle`)** 는 4번 결론(화면 캡처)의 핵심 재료가 됐다.

**3번(이벤트 구독) — 불가능, 소스 자체의 결함으로 확인.** `EVENT_OBJECT_NAMECHANGE`가
`Windows.UI.Input.InputSite.WindowClass`에서 유일하게 관련 있는 이벤트였지만:
- `WINEVENT_OUTOFCONTEXT` 훅: 신중하게 한 번씩 전환해도 비결정적(2/3~1/4 적중, `LangBarPoc4`/`6`).
  이벤트가 뜨더라도 `AccessibleObjectFromEvent`로 실제 값 조회는 `E_FAIL`(`LangBarPoc5`/`6`).
- **결정적 실험**: 마샬링 유실 가능성을 원천 차단하는 `WINEVENT_INCONTEXT`(콜백 DLL을
  `explorer.exe`에 실제로 로드시켜 그 프로세스 안에서 동기 실행 — `running-in-pid`로 확인함,
  `LangBarPoc8`/`LangBarHookDll`)로도 **10번 전환 중 1번(10%)만 잡혔다.** 즉 문제는 훅 전달
  방식이 아니라 **`explorer.exe` 자신이 이 접근성 이벤트를 매번 일관되게 발생시키지 않는다**는
  것 — 어떤 훅 기법으로도 못 고치는 소스 레벨 결함.

**4번(폴링 기반 화면 캡처) — 기술적으로는 검증 완료.** UIA로 찾은 좌표로 `BitBlt` 1회성
캡처가 두 상태 모두 정확했고(`LangBarPoc7`, DPI awareness 미설정 시 좌표가 어긋나는 함정이
있었음 — `SetProcessDpiAwarenessContext`로 해결), 1초 폴링 + 기준 이미지 픽셀 비교로 상태
판정까지 실측 검증했다(`LangBarPoc9`): 전환 후 최대 1초 이내 정확히 반영, 오판 없음(정답 쪽
`diff=0`, 오답 쪽 `diff≈52000`으로 압도적 차이), 폴링 1회당 4~33ms(평균 10ms대) — 1초 주기
기준 CPU 점유율 약 1~3%.

**최종 판단(사용자, 2026-08-15): 이 이슈는 폐기하지 않고 "최후의 대안"으로 격하해 보류한다.**
Windows가 어딘가에는 진짜 이벤트 기반 경로를 제공할 거라는 신뢰를 아직 완전히 거두지 않았고,
다른 경로(우선 [issue 02](02-windows-settings-and-policy.md) — 원래 TIP `Activate()` 회귀
원인 추적)를 계속 파본 뒤, 그것도 끝내 실패하면 그때 이 폴링 기반 화면 캡처 방식을 채택한다.
즉 이 이슈가 남긴 자산은 "실패"가 아니라 "검증된 안전망 하나" — CLAUDE.md 3절의 "폴링이 아닌
이벤트 기반" 원칙과는 충돌하므로, 실제로 채택하게 되면 그 원칙 자체를 먼저 재개정해야 한다.
