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

(아직 없음 — 이 맵은 2026-08-14에 막 시작됐다)

## Not yet specified

- 회귀 원인이 끝내 안 밝혀지면 대안 아키텍처([issue 04](issues/04-taskbar-indicator-observation.md))로
  완전히 갈아탈지, 절충([issue 03](issues/03-selection-required-nongoal-conflict.md))으로 갈지는
  각 리서치 결과가 나온 뒤 사용자와 다시 상의한다.

## Out of scope

(`ime-focus-accuracy` 맵에서 승계) DirectX 등 TSF/IMM을 전혀 안 타는 앱 — 관찰 가능한 신호
자체가 없어 이 프로젝트의 접근으로 달성 불가능. 제3의 "무포커스" UI 상태 — "마지막 상태 유지"로
충분.
