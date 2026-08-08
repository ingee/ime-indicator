# IME 상태 표시 정확도 — 포커스 인식 아키텍처 결정

Label: wayfinder:map

## Destination

`cmd.exe`류 클래식 콘솔(`conhost.exe`)과 Excel을 포함해, TSF 경로가 있는 모든 앱에서 한/영 상태를
오차 없이 표시하기 위한 아키텍처를 결정한다. 구체적으로 (1) UI가 "지금 TSF 포커스를 가진 프로세스"를
판단하는 방법을 TSF 자체 신호 기반으로 바꿀지·어떻게 바꿀지, (2) 우리 TIP이 Office류 앱에서
`Activate()`조차 안 불리는 원인과 해결책을 확정한다. 완료 시점은 코드가 아니라 **CLAUDE.md 4절을
갱신할 준비가 된 결정 상태**다 — 구현 자체는 `CLAUDE_worklist.md` 3절의 미완료 항목("엔드투엔드 수동
시나리오 검증")으로 이관한다. 이 맵은 새 이니셔티브가 아니라 그 항목을 막고 있는 결정을 정리하는
것이다.

## Notes

- 폴링은 최대한 피한다 — 절대 불변은 아니지만 강한 기본 원칙(다른 방법이 정말 없을 때만 최후 수단).
- TIP DLL 프로세스 주입 + IPC push 아키텍처 자체는 유지한다 — Windows 자체 입력 표시기도 같은 구조적
  패턴(프로세스마다 클라이언트 로드 + 중앙 relay)을 쓴다는 게 조사로 확인됐다(Google Project Zero,
  Tavis Ormandy, 2019). 재검토 대상이 아니다.
- 무포커스/논텍스트 상태는 "마지막 상태 유지"로 확정 — 제3 UI 상태 추가 안 함(구현 용이성 우선,
  CLAUDE.md 5절 변경 없음).
- 관련 스킬: 리서치 티켓은 `/research`, 필요시 `/domain-modeling`.
- 결정이 나오면 CLAUDE.md 4절 갱신 + `CLAUDE_worklist.md` 3절 구현으로 이어진다.

## Decisions so far

- [TSF 네이티브 포커스 신호로 UI의 포커스 판단을 대체/보완할 수 있는가](issues/01-tsf-native-focus-signal.md) —
  채택(병행/보완, 완전 대체 아님). TIP이 `OnSetFocus`로 포커스 획득/상실을 IPC로 추가 보고,
  UI는 `SetWinEventHook`을 유지하되 테이블에 없거나 더 최신 포커스 보고가 있으면 그 PID 우선.
  `conhost.exe`에서 실제 발화 여부는 미검증 — 구현 전 프로토타입에서 최우선 검증.
- [Office(Excel)에서 우리 TIP의 Activate()가 안 불리는 원인](issues/02-office-tip-activation-gap.md) —
  근본 원인 미확정(유력 가설: Excel 셀 에디터가 TSF-unaware라 Transitory Context 경로를 타 선택된
  TIP만 로드). `TF_IPPMF_ENABLEPROFILE` 저위험 실험 권고(선택된 프로필 되기는 비목표와 충돌해 배제).
- **최종 통합 결정(사용자 확인, 2026-08-08)**: A는 그대로 채택해 구현. B는 리서치가 제안한
  "실패 시 알려진 제약으로 수용"을 그대로 따르지 않고, **실패하면 자동으로 제약 처리하지 말고
  다시 상의한다**로 수정 — 이 프로젝트는 앞서(2026-08-07 세션) "알려진 제약으로 문서화하고
  넘어가는 것"을 사용자가 명시적으로 거부한 전례가 있어(CLAUDE_worklist.md 세션 요약 참고),
  그 원칙과 일치시킴. 이걸로 destination 도달 — CLAUDE.md 4절 갱신 + `CLAUDE_worklist.md` 3절
  구현 항목화로 넘어간다.

## Not yet specified

- `TF_IPPMF_ENABLEPROFILE` 실험이 실패할 경우 어떤 대안을 상의할지 — 지금은 실패 여부도, 실패
  원인도 모르므로 구체화 불가. 실험 결과가 나와야 논의 가능(이 항목이 A/B 구현 착수 자체를
  막지는 않음 — 실험은 저위험이라 시도 자체는 바로 진행 가능).

## Out of scope

- DirectX 등 TSF/IMM을 전혀 안 타는 앱 — 관찰 가능한 신호 자체가 없어 이 프로젝트의 접근으로
  달성 불가능(사용자 확인, 2026-08-08).
- 제3의 "무포커스" UI 상태 — 구현 용이성 우선으로 기각, "마지막 상태 유지"로 충분(사용자 확인,
  2026-08-08).
