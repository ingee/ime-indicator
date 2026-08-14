Type: research
Status: open

## Question

`EVENT_SYSTEM_FOREGROUND`를 `WINEVENT_INCONTEXT`로 훅 걸어서, 포커스가 바뀔 때마다 그
순간 포커스를 얻은 프로세스 안에 우리 코드를 직접 주입하고, 그 안에서 TIP 자동 로드를 기다리지
않고 직접 Compartment 이벤트를 구독하는 아키텍처가 가능한가?

### 배경

- [issue 04](04-taskbar-indicator-observation.md)에서 "참가자가 안 되고 바깥에서 관찰"하는
  세 경로(공식 TSF API, UI Automation, WinEvent 구독)가 전부 막힌 걸 확인했다 — TSF는 원래
  비참가자의 접근을 지원하지 않는 설계라, 관찰 대상을 "포커스된 창 하나"로 좁혀도 이 벽은
  그대로였다(`ITfLangBarItemMgr::GetThreadLangBarItemMgr`는 스레드 개수와 무관하게 "남의
  스레드"면 항상 `E_FAIL`).
- 반면 원래 TIP 방식(ADR-0003)은 "참가자가 되는" 모델이라 이 벽 자체가 없고, 8/7까지는 실제로
  정확·이벤트 기반으로 정상 동작했다. 지금 막힌 건 딱 한 지점 — **Windows가 우리 TIP을
  자동으로 각 프로세스에 로드하고 `Activate()`해주는 그 진입점**이 왜인지 대부분 프로세스에서
  더는 안 됨([issue 02](02-windows-settings-and-policy.md), 원인 미해결).
- [issue 04](04-taskbar-indicator-observation.md) 조사 중 `WINEVENT_INCONTEXT`로 우리 DLL을
  `explorer.exe`에 실제로 로드시키는 데 성공했다(`LangBarPoc8`,
  `prototype/langbar-observation-poc-throwaway` 브랜치 — `running-in-pid`로 실제 대상
  프로세스 안에서 콜백이 도는 것까지 확인). 다만 그때 쓴 이벤트(`EVENT_OBJECT_NAMECHANGE`)는
  `explorer.exe` 자신이 비일관적으로만 발생시켜서 신뢰도가 낮았다 — 이건 주입 메커니즘 자체의
  문제가 아니라 그 특정 이벤트 소스의 문제였다.
- **아이디어(사용자 제안, 2026-08-15)**: 같은 `WINEVENT_INCONTEXT` 주입 메커니즘을, 신뢰도
  낮았던 `NAMECHANGE` 대신 **훨씬 근본적인 시스템 이벤트인 `EVENT_SYSTEM_FOREGROUND`**(Alt-Tab
  전환창·작업표시줄 자체도 의존하는 이벤트)에 걸면, 포커스가 바뀔 때마다 그 프로세스 안에 우리
  코드가 심어진다. 그 안에서는 TIP 자동 로드 성패와 무관하게 직접
  `CoCreateInstance(CLSID_TF_ThreadMgr)` → `GetFocus()` → Compartment 구독을 걸 수 있다 —
  즉 "왜 자동 `Activate()`가 고장났는지" 미스터리를 풀 필요 없이 **그 진입점 자체를 우회**한다.
  범위도 "시스템 전역"에서 "지금 포커스된 프로세스 하나"로 자연히 좁혀진다.
- **남는 위험(성립하더라도)**: 포커스 전환마다 그 앱에 우리 DLL이 실제로 주입되므로, 버그가
  나면 지금 쓰는 임의의 앱이 죽을 수 있다. "포커스가 바뀔 때마다 다른 프로세스에 몰래 들어가는"
  동작 패턴이 키로거류와 모양이 비슷해 백신/EDR 휴리스틱에 걸리기 쉽다(참고:
  [issue 01](01-security-software-investigation.md)에서 조사했던 Citrix App Protection이
  정확히 이런 패턴을 잡는 도구). 32/64비트 프로세스가 섞여 있으면 DLL도 양쪽 다 준비해야 한다.

### 조사할 것

1. `EVENT_SYSTEM_FOREGROUND`를 `WINEVENT_INCONTEXT`로 훅 걸었을 때, 콜백이 실제로 포커스를
   얻은 그 프로세스 "안에서" 실행되는지 확인(`LangBarPoc8`처럼 `GetCurrentProcessId()`로 증명).
2. 이 이벤트가 매 포커스 전환마다 신뢰성 있게 뜨는지 — 오늘 `NAMECHANGE`처럼 소스 자체가
   비일관적인지, 아니면 훨씬 근본적인 시스템 이벤트라 안정적인지 실측.
3. 주입된 콜백이 실행되는 스레드가 그 창을 소유한 UI 스레드와 일치하는지 — TSF 작업은 스레드
   어피니티가 있어서 다른 스레드에서 실행되면 못 쓴다.
4. 성공하면, 그 안에서 실제로 `CLSID_TF_ThreadMgr` 생성 → `GetFocus()` → Compartment 구독까지
   이어붙여서 8/7 이전에 정상 동작했던 로직이 이 새 진입점에서도 그대로 동작하는지 확인.
5. 32/64비트 프로세스 혼재 시 대응 방안(양쪽 DLL 준비 필요성) 정리.

### 결론에 포함할 것

실현 가능성 판정(가능/불가능/조건부) + 성립하면 TIP 회귀(issue 02) 원인 규명 자체를 우회할 수
있는지 여부 + 남는 리스크(크래시 블라스트 반경, AV/EDR 오탐 등) 정리.
