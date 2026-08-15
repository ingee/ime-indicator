# IME 상태 감지 아키텍처 재검토 — TIP 회귀 원인 규명 + 대안 후보

Label: wayfinder:map

## Destination

세 가지 목표(아래 Notes 참고)를 오차 없이 달성하는 아키텍처를 다시 확정한다. TIP 전역 주입
방식(ADR-0003)은 2026-08-07까지 정상 동작을 실측으로 확인했으나, 2026-08-08 이후 원인 불명의
회귀(`Activate()`가 대부분 프로세스에서 전혀 호출되지 않음)로 막혀 있다. 2026-08-14 세 세션에
걸친 광범위한 조사로도 근본 원인을 못 찾았다. 이 맵은 (1) 그 회귀의 남은 원인 후보들과 (2) TIP
자체를 우회하는 대안 아키텍처 후보들을 함께 관리해, 여러 세션에 걸쳐 하나씩 검증할 수 있게 한다.
완료 시점은 "왜 안 됐는지" 완전 규명이 아니라 — **CLAUDE.md 3~4절을 다시 갱신할 준비가 된 결정
상태**(TIP을 계속 쓸지, 대안으로 갈지, 절충할지)다.

## Notes

- **목표 재정의**(2026-08-14, 사용자 확정) — CLAUDE.md 3절의 "목표"는 이 세 문장이다. 특정
  구현 수단(TIP 등)을 목표 문장에 끼워넣지 않는다.
  1. 시스템의 한/영 입력 상태를 파악한다
  2. 시스템의 한/영 입력 상태 변화 이벤트를 획득한다
  3. 시스템의 한/영 입력 상태를 모든 모니터에 표시한다
- 기존 비목표("IME 자체 동작 가로채기 금지")는 지금 **재검토 대상**이다([issue 03](issues/03-selection-required-nongoal-conflict.md)) — 확정된 제약이 아니라 열린 질문으로 다룬다.
- **기각된 가설**(2026-08-14, 재검토 불필요): CLSID 오염, `ENABLED`/`ACTIVE` 레지스트리 플래그
  (원래 계정 + 완전히 새로 만든 테스트 계정 `ime-test`에서 각각 재확인), `ctfmon.exe` 재시작,
  재부팅, 등록 코드 자체의 변경(`git diff be78ffe HEAD -- src/ImeIndicatorTip/Registration.cpp`가
  완전히 비어있음 — 8/7 마지막 정상 동작 시점과 지금 코드가 100% 동일. 프로필 GUID도 최초
  프로토타입 `TipPoc6`부터 항상 CLSID와 함께 존재했음을 브랜치 히스토리로 확인).
- **새로 확인된 핵심 사실이자 미해결 모순**: 지금 이 시스템에서는 **선택 안 된(비활성) TIP은
  `Activate()`가 자동으로 호출되지 않는다** — 완전히 새로 만든 계정(`ime-test`)에서 실제로
  Win+Space로 선택한 순간에만 로드됨을 실측 확인(2026-08-14). 그런데 2026-08-07에는 Microsoft
  IME가 계속 선택된 채로 실제 한글 입력이 정상 동작하면서 동시에 우리 TIP도 `Activate()`됐다
  (인디케이터가 정확히 따라갔음, 커밋 `be78ffe`). 즉 "선택 안 된 관찰자도 로드된다"던 그때의
  실측(ADR-0005)과 지금의 실측이 정면으로 모순된다 — **이 모순은 아직 안 풀렸다.**
- `ENABLED` 레지스트리 플래그(작업표시줄 "한" 표시 여부)는 `Activate()` 호출과 **무관하다**는
  게 두 계정에서 재확인됐다 — 한때 유력하게 의심했던 방향이지만 최종적으로 기각.
- **Procmon은 이 PC에서 쓰지 말 것** — 정체불명의 보호 프로그램(Themida 추정)과 충돌해 대상
  프로세스를 정지시킨다. 대신 ETW(`logman`/`tracerpt`, `registry,process,img` 키워드)를 표준
  진단 수단으로 쓴다:
  ```
  logman start "NT Kernel Logger" -p "Windows Kernel Trace" "(registry,process)" -ct perf -o <etl경로> -ets
  logman stop "NT Kernel Logger" -ets
  tracerpt <etl경로> -o <csv경로> -of CSV -summary <summary경로> -y
  ```
- 이 PC는 회사 업무도 보는 **개인 소유** PC다(강력한 중앙 통제 아래는 아님). `MagicLine4NX`
  (한국 기업환경에서 흔한 키보드보안/VPN), Citrix 관련 프로세스(`concentr.exe`, `Receiver.exe`,
  `wfcrun32.exe`)가 설치돼 있음 — 아직 조사 안 됨([issue 01](issues/01-security-software-investigation.md)).
- **ADR 충돌 — 반드시 알림**:
  - **ADR-0003**("TIP DLL을 시스템에 등록해 텍스트 입력을 다루는 거의 모든 프로세스에
    로드시킨다")의 전제가 지금 실측(선택돼야 로드됨)과 부딪힌다. 폐기는 아니지만, 이 맵의
    관련 이슈가 결론 날 때까지 "검증된 사실"이 아니라 "가설"로 강등해서 다룬다.
  - **ADR-0006**의 Excel `TF_IPPMF_ENABLEPROFILE` "저위험" 실험 권고는 재검토 필요 —
    사용자의 타임라인 회상(8/8 밤 이 실험 직후 회귀 시작)이 유력한 원인 후보라서, 근본
    원인이 밝혀지기 전까지는 **다시 시도하지 않는다**([issue 06](issues/06-cmd-onsetfocus-and-excel-reverification.md) 참고).
- 관련 스킬: 리서치 티켓은 `/research`, 필요시 `/domain-modeling`.
- 선행 맵 `.scratch/ime-focus-accuracy/`는 destination(ADR-0006)에 도달했다고 표시돼 있었지만,
  그 destination 자체가 이번 회귀로 다시 불확실해졌다 — 그 맵의 미해결 구현 항목(`issues/01`,
  `02`)은 이 맵의 [issue 06](issues/06-cmd-onsetfocus-and-excel-reverification.md)이 계승한다.

## Decisions so far

- **2026-08-15, [issue 01](issues/01-security-software-investigation.md) resolved — 보안/VPN
  소프트웨어 관련성 낮음.** MagicLine4NX·Citrix 전 제품군 바이너리/이벤트 로그 모두 8/6~8/9
  구간에 변화 없음(업데이트 가설 기각). Citrix App Protection(`epinject.sys`/`entryprotect.sys`)은
  실재하는 anti-keylogging 커널 드라이버이지만, 공식 문서상 "보호 대상 창이 포커스를 가졌을 때만
  활성화" — 회귀 재현 대상(Notepad/cmd.exe/Excel)이 Citrix 게시 앱이 아니라 스코프 밖일
  가능성이 높음. 실측 종료 재테스트는 생략(사용자 상의 후 우선순위 낮음 판단). 다음은 issue 02.
- **2026-08-15, [issue 04](issues/04-taskbar-indicator-observation.md) resolved — "최후의
  대안"으로 보류.** `prototype/langbar-observation-poc-throwaway` 브랜치에서 9개 프로토타입으로
  실측: 공식 TSF API(`ITfLangBarItemMgr`)는 크로스 프로세스 조회 불가, UI Automation은 상태를
  노출 안 함(좌표 조회에는 유용), 이벤트 구독(`EVENT_OBJECT_NAMECHANGE`)은 `WINEVENT_INCONTEXT`로
  `explorer.exe`에 콜백을 직접 로드해도 10번 중 1번만 잡힘 — 훅 방식이 아니라 `explorer.exe`
  자신이 이벤트를 일관되게 안 낸다는 소스 레벨 결함으로 확정. 반면 폴링 기반 화면 캡처(UIA로 찾은
  좌표 + 1초 폴링 + 픽셀 비교)는 정확성·비용(1회당 ~10ms, CPU ~1~3%) 모두 실측 검증됨. 사용자
  판단: 폐기하지 않고 최후의 안전망으로 보류 — Windows가 어딘가엔 진짜 이벤트를 줄 거라는 신뢰를
  아직 안 거두고, issue 02(TIP 회귀 원인 추적)를 먼저 계속 판다. 이것마저 실패하면 그때 폴링을
  채택하되, 그 시점엔 CLAUDE.md 3절의 "폴링 금지" 원칙 자체를 재개정해야 함.
- **2026-08-15, [issue 07](issues/07-focus-triggered-incontext-injection.md) resolved — 실현
  가능성 확인, 유력 후보로 격상.** `prototype/langbar-observation-poc-throwaway` 브랜치에
  `LangBarPoc10`/`LangBarPoc11`로 5단계 전부 실측: (1) 비트니스만 맞으면 `EVENT_SYSTEM_FOREGROUND`
  + `WINEVENT_INCONTEXT`로 포커스 얻은 프로세스에 인프로세스 주입 100% 성공(비Office 20건+Office
  3건), (2) 드롭 없이 신뢰성 양호, (3) 콜백 스레드가 UI 스레드와 대체로 일치하나 예외 1건(설계 시
  마샬링 필요), (4) **핵심 검증**: 그 콜백 안에서 직접 `CoCreateInstance(CLSID_TF_ThreadMgr)` →
  `Activate()` → Compartment `AdviseSink`까지 걸어도 TSF가 정상 참가자로 받아들여 8/7 이전과
  동일하게 `OnChange`가 정확히 잡힘(Notepad++에서 한/영 20회 전환 전부 성공), (5) x64+x86 훅을
  동시에 띄우면 32비트 Office(Excel/Word/PowerPoint)까지 커버됨 — **TIP 방식이 한 번도 못 했던
  것**(TIP은 x64 전용으로만 빌드돼 왔음, 8/8 회귀와 무관한 원래부터의 공백). 결론: 이 아키텍처는
  issue 02(TIP `Activate()` 회귀 원인)를 몰라도 되게 만들며, 이슈 04의 모든 대안보다 안정적이다.
  남는 위험은 스레드 마샬링 미구현, 크래시 블라스트 반경, AV/EDR 오탐 소지, 포커스별 재구독
  로직(ADR-0002 방식) 미검증 — 상세는 issue 07 결론 참고.
- **2026-08-15, [ADR-0007](../../docs/adr/0007-focus-triggered-incontext-injection.md) 채택
  (destination 도달) — 사용자 확정.** issue 07의 아키텍처를 새로운 핵심 감지 메커니즘으로
  정식 채택. CLAUDE.md 4절을 이 결정에 맞춰 갱신, ADR-0005/0006에 `superseded by ADR-0007`
  마킹(단 두 문서가 남긴 실측 사실은 계속 유효). [issue 02](issues/02-windows-settings-and-policy.md)
  ("TIP `Activate()` 회귀 원인")는 더 이상 풀 이유가 없어져 "무관해짐"으로 resolved 처리.
  **이 맵은 destination에 도달했다** — 이후 세션에서는 참고용으로만 남는다. 실제 프로덕션
  구현(`src/`에 새 아키텍처 반영)은 아직 착수 전이며 별도 작업으로 넘어간다.

## Not yet specified

- **ADR-0007 채택으로 재검토가 필요해진 기존 이슈**(다음 세션에 우선 정리):
  - [issue 03](issues/03-selection-required-nongoal-conflict.md)("선택된 프로필이어야만
    관찰 가능한가")은 전제 자체가 무너졌을 가능성이 높다 — 새 아키텍처는 TIP으로 등록되거나
    선택될 필요 없이, 임의의 주입된 코드가 직접 `CoCreateInstance(CLSID_TF_ThreadMgr)`을 불러
    관찰에 성공했다(issue 07 4번). 재확인 후 moot 처리할지 판단 필요.
  - [issue 06](issues/06-cmd-onsetfocus-and-excel-reverification.md)의 Excel 부분
    (`TF_IPPMF_ENABLEPROFILE` 실험)은 불필요해졌다(ADR-0007 Consequences 참고). 다만
    `cmd.exe`/`conhost.exe` PID 스왑 문제(ADR-0005 Update)는 **새 아키텍처에서 재현되는지
    아직 실측 안 함** — `EVENT_SYSTEM_FOREGROUND`의 `hwnd` 소유 PID가 `cmd.exe`로 보고되는
    상황에서 실제 주입이 `cmd.exe`/`conhost.exe` 중 어디로 되는지 확인 필요. issue 06을
    이 항목만 남기고 재작성할지 새 이슈로 분리할지는 다음 세션에 결정.
- 실제 구현 착수 시 순서: ADR-0007 Consequences의 남은 위험(스레드 마샬링, 포커스별 재구독,
  x64+x86 프로세스 배포 구조) 중 어디부터 `src/`에 반영할지 다음 세션에 계획.

## Out of scope

(`ime-focus-accuracy` 맵에서 승계) DirectX 등 TSF/IMM을 전혀 안 타는 앱 — 관찰 가능한 신호
자체가 없어 이 프로젝트의 접근으로 달성 불가능. 제3의 "무포커스" UI 상태 — "마지막 상태 유지"로
충분.
