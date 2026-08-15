Type: research
Status: resolved

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

## 조사 결과 — 1~3번 (2026-08-15)

`prototype/langbar-observation-poc-throwaway` 브랜치에 `LangBarPoc10`(x64 시스템 전역
`WINEVENT_INCONTEXT` 훅, `idProcess=0`으로 `EVENT_SYSTEM_FOREGROUND` 구독) +
`LangBarHookDll.dll`에 `ForegroundHookProc` 추가(커밋 `d7c0c31`). 두 라운드 실측(1라운드:
탐색기·Firefox·Notepad++·메모장·mintty, 2라운드: Excel·Word·PowerPoint 포함 재실행), 매
콜백마다 `windowPid`/`windowTid`(=`GetWindowThreadProcessId`) vs `running-in-pid`(=콜백
안에서 `GetCurrentProcessId()`)/`callback-tid`(=`GetCurrentThreadId()`) 비교.

**1. 콜백이 실제로 포커스 얻은 프로세스 "안에서" 도는가 — 조건부 성립.**
- 비Office 앱(탐색기 `explorer.exe`, Firefox, Notepad++, mintty, WinUI/COM 호스트 창 등,
  두 라운드 합산 20건 이상): `running-in-pid == windowPid` 전부 일치. 예외 없음.
- **Excel/Word/PowerPoint(전부 Office14 = Office 2010, `C:\Program Files (x86)\...` 설치 —
  32비트) 3건 전부 실패.** `running-in-pid`가 매번 25088로 찍혔는데, 이는 실제 Office 창의
  PID(9572/27496/3004 — 서로 다름)가 아니라 **훅을 설치한 `LangBarPoc10.exe` 자신의 PID**였다.
  원인: 훅 설치 프로세스와 `LangBarHookDll.dll`을 x64로 빌드했는데 Office는 32비트라 인프로세스
  주입이 애초에 불가능 — Windows가 `WINEVENT_INCONTEXT` 요청을 조용히 out-of-context 방식으로
  강등시켜, 콜백이 대상 프로세스가 아니라 훅 설치 프로세스 안에서 대신 실행된 것으로 판단된다.
  **이슈 등록 당시 "남는 위험" 5번(32/64비트 혼재)이 이론적 우려가 아니라 실측으로 확정된
  장애물임을 확인** — 그것도 회귀 재현 대상이었던 바로 그 Excel에서.

**2. 매 포커스 전환마다 신뢰성 있게 뜨는가 — 양호.** 두 라운드 모두, 실제로 전환한 서로 다른
최상위 창마다 최소 1개 이상의 FOREGROUND 이벤트가 잡혔다. 드롭으로 의심되는 case 없음. (탐색기의
`ForegroundStaging`/`XamlExplorerHostIslandWindow`, Office의 `MsoSplash` 등은 셸/앱 초기화
과정의 과도기 창이 추가로 찍힌 것이라 실제 앱 전환과는 별개 — 중복이지 드롭이 아니다.) issue
04의 `EVENT_OBJECT_NAMECHANGE`(10번 중 1번)와 대비되는 안정성.

**3. 콜백 스레드가 그 창의 UI 스레드와 일치하는가 — 대체로 일치하나 예외 확인.** 인프로세스로
확인된 건(비Office) 중 1건(메모장, pid 3732)에서 `callback-tid=24516`인데 `windowTid=22376`로
**불일치**했다. 나머지는 전부 일치. 표본이 작아 재현 조건은 특정 못 함 — 4번 단계(TSF 연결) 설계
시 `windowTid`와 `GetCurrentThreadId()`를 매번 비교해, 다르면 그 UI 스레드로 작업을 마샬링하는
경로를 기본으로 넣어야 한다(항상 일치한다고 가정하면 안 됨).

**중간 결론(수정)**: 주입 메커니즘 자체(질문 1, 2)는 **비Office 프로세스에서는** issue 04의
대안들보다 훨씬 안정적임이 확인됐다. 32비트 Office는 x64 빌드로는 실패했지만, 원인이 정확히
비트니스 미스매치였다는 가설을 아래 5번 실측으로 확정 검증했다.

## 조사 결과 — 5번, 32/64비트 혼재 대응 (2026-08-15)

`LangBarHookDll-x86.dll` + `LangBarPoc10-x86.exe`(커밋 `56e6d0d`)로 동일 실험을 x86으로
재실행(vcvarsall.bat x86). 결과, **x64/x86 실패가 정확히 대칭적으로 예측대로 일어남을 확인**:

- **x86 훅으로 Excel(`XLMAIN`)/Word(`OpusApp`)/PowerPoint(`PPTFrameClass`) 3개 앱 전부
  인프로세스 성공.** `running-in-pid == windowPid`, `callback-tid == windowTid` 모두 일치.
- 반대로 **64비트 앱(탐색기, Notepad++, Firefox, mintty, WinUI CoreWindow, 그림판)은 x86
  훅으로는 전부 실패** — `running-in-pid`가 매번 `3168`(x86 로더 `LangBarPoc10-x86.exe` 자기
  자신의 PID, `Get-Process`로 확인)로 찍힘. x64 훅이 32비트 Office에서 실패했던 것과 정확히
  거울상.

**결론**: `WINEVENT_INCONTEXT`는 "훅 설치 프로세스와 같은 비트니스의 대상에만 인프로세스로
주입된다"는 규칙이 양방향으로 실측 확정됐다. 대응은 간단하다 — **x64 훅 프로세스와 x86 훅
프로세스를 항상 동시에 띄운다.** 각자 자기 비트니스의 프로세스만 담당하므로 둘을 합치면 시스템의
모든 프로세스를 커버한다(이론상 ARM64 등 제3의 아키텍처가 섞이면 그만큼 훅이 더 필요하지만, 이
PC는 x64/x86만 확인되면 충분). 구현 시 두 프로세스(또는 두 훅을 가진 하나의 관리 프로세스 +
아키텍처별 자식 프로세스)가 같은 IME 상태 저장소로 결과를 모으는 구조가 필요하다.

**1~3, 5번 종합 결론**: 주입 메커니즘(질문 1)은 비트니스만 맞으면 인프로세스 실행이 100%
확정적이었고(비Office 20건 + Office 3건 = 23건 전부), 신뢰성(질문 2)도 드롭 없이 양호했다.
스레드 일치(질문 3)는 대체로 성립하나 예외 1건(메모장)이 있어 4번 설계 시 방어 코드가 필요하다.
**남은 건 4번(실제 Compartment 연결)뿐이며, 1~3·5번 결과로 볼 때 4번을 막을 만한 새로운 장애물은
지금까지 발견되지 않았다.**

**정정(사용자 확인, 2026-08-15)**: 위에서 "8/7 이전 TIP가 32비트 Excel에서 실제로 작동했었는지
확인해볼 가치가 있다"고 적었던 건 틀린 추측이었다 — **TIP 방식은 32비트 Excel에서 단 한 번도
작동한 적이 없다.** 기존 `src/ImeIndicatorTip`가 x64 전용으로만 빌드돼 왔기 때문에, 이건 8/8
회귀로 새로 생긴 문제가 아니라 TIP 접근 자체에 처음부터 있었던, 그동안 발견되지 않았던 공백이다.
즉 이슈 07의 x86 훅은 "회귀를 우회"하는 것을 넘어 **TIP 방식이 한 번도 못 했던 32비트 Office
커버리지를 이번에 처음 달성**한 것 — 대안으로서의 값어치가 처음 평가했을 때보다 크다.

## 조사 결과 — 4번, TSF Compartment 연결 (2026-08-15)

`LangBarHookDll11.dll`(`CompartmentHookProc`) + `LangBarPoc11.exe`(커밋 `003ab8f`) — 스레드가
일치하는 경우(3번 조건) `EVENT_SYSTEM_FOREGROUND` 콜백 안에서 직접
`CoCreateInstance(CLSID_TF_ThreadMgr)` → `Activate()` → `ITfCompartmentMgr` QI →
`GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE)` → `AdviseSink`까지 수행하도록
`src/ImeIndicatorTip/ImeStateTip.cpp`의 "스레드 매니저 자신을 QI" 로직을 그대로 이식.

**결과: 전부 성공.** Notepad++(pid 27224)로 전환한 뒤 그 안에 머무르며 한/영을 20회 전환 —
`OnChange`가 **누락 없이, 매번 정확한 값으로**(0/1 번갈아) 전부 잡혔다. `ThreadMgr::Activate`,
`GetCompartment`, `AdviseSink` 전부 첫 시도에 성공(등록된 TIP이 전혀 아닌데도). 탐색기/conhost/
mintty에서도 `Activate`~`AdviseSink`까지는 성공(그 창들에서는 한/영을 안 건드려 `OnChange`
케이스는 없었지만 구독 자체는 됨).

**의미**: "TSF가 우리를 `Activate()`해주는 걸 기다리지 않고, 포커스 전환 시점에 우리가 직접
`ThreadMgr`를 만들어 구독을 걸어도, TSF는 이를 정상적인 참가자로 받아들이고 8/7 이전과 동일하게
동작한다"가 실측으로 확정됐다. issue 02가 풀지 못한 "왜 Windows가 우리 TIP을 자동으로
`Activate()`해주지 않는가"라는 미스터리를 **몰라도 되게** 만드는, 이슈 등록 당시의 가설이 그대로
들어맞았다.

## 결론

**실현 가능성: 가능.** 조사할 것 1~5번 전부, 별도 전제조건 없이 그대로 성립했다.

- **TIP 회귀(issue 02) 원인 규명을 완전히 우회한다.** 이 아키텍처는 Windows의 자동 TIP
  `Activate()` 진입점을 아예 쓰지 않으므로, 그게 왜 고장났는지 몰라도 동작한다.
- **TIP 방식이 한 번도 못 했던 것(32비트 Office 커버리지)을 추가로 달성한다** — x64+x86 훅을
  동시에 띄우면 됨(질문 5, 실측 검증 완료).
- 신뢰성(질문 2)은 issue 04에서 검토한 모든 대안(`ITfLangBarItemMgr` 크로스프로세스 조회 불가,
  UI Automation 상태 미노출, `NAMECHANGE` 10번 중 1번)보다 뚜렷하게 낫다.
- **남는 위험**:
  1. 스레드 불일치 예외(질문 3, 메모장에서 1건 관측) — 프로덕션 구현 시 `windowTid`와
     `GetCurrentThreadId()`를 비교해 다르면 그 UI 스레드로 마샬링하는 경로가 필요하다(이번
     프로토타입은 불일치 시 그냥 건너뜀 — 실제 구현엔 못 넣고 지나칠 항목).
  2. 크래시 블라스트 반경 — 포커스 전환마다 임의의 앱 프로세스에 우리 DLL이 실제로 로드되므로,
     버그가 나면 그 순간 쓰고 있던 앱이 죽을 수 있다.
  3. AV/EDR 오탐 — "포커스가 바뀔 때마다 다른 프로세스에 몰래 들어가는" 패턴은 키로거류와
     휴리스틱이 겹친다(issue 01에서 조사한 Citrix App Protection이 정확히 이런 패턴을 잡는
     도구).
  4. 이번 프로토타입은 프로세스당 최초 1회만 구독하고 이후 재진입은 스킵(`g_subscribed`) —
     실제 구현은 창/컨텍스트 전환마다 ADR-0002 방식(`OnSetFocus`로 구독 교체)을 이 진입점 위에
     다시 얹어야 한다. 아직 실측 안 함.
  5. `threadMgr`을 의도적으로 leak시켰다(Release 시 구독이 같이 죽을 가능성 회피) — 실제 구현은
     프로세스 종료/재주입 시점의 정리 전략이 필요하다.

**다음 결정 지점**: `.scratch/ime-detection-strategy/map.md`의 destination(CLAUDE.md 3~4절
갱신)에 반영할 선택지 — 이 아키텍처를 정식 채택할지, issue 02(회귀 원인 규명)를 계속 병행할지는
사용자와 상의 필요.
