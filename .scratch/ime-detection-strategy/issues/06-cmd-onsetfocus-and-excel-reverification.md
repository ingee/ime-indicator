Type: task
Status: open
Blocked by: 01, 02

## Question

`cmd.exe`(`conhost.exe`) 미반영 문제(`OnSetFocus` 구독)와 Excel 미반영 문제
(`TF_IPPMF_ENABLEPROFILE` 실험)를 계속 진행한다 — 단, 지금의 `Activate()` 회귀가 풀리기
전까지는 착수 자체가 무의미하다.

### 배경

- 선행 맵 `.scratch/ime-focus-accuracy/`의 미해결 구현 항목(`issues/01-tsf-native-focus-signal.md`,
  `issues/02-office-tip-activation-gap.md`)을 계승한다. 그 맵은 destination(ADR-0006)에
  도달했다고 표시돼 있었지만, 이번 회귀로 그 destination 자체가 다시 불확실해졌다.
- `CLAUDE_worklist.md` 3절의 "TIP: TSF 포커스 신호 구독", "Excel: `TF_IPPMF_ENABLEPROFILE`
  저위험 실험", "엔드투엔드 시나리오 재검증" 항목이 여기 해당한다(그 파일은 보관용으로
  전환됐다).
- **중요**: Excel `TF_IPPMF_ENABLEPROFILE` 실험은 지금 회귀의 유력한 원인 후보 자체다 —
  사용자가 회상한 타임라인(8/8 밤 이 실험 직후 "한" 표시 등장 + 회귀 시작)이 근거다. 그러니
  **회귀 원인이 밝혀지기 전까지는 이 실험을 다시 시도하지 않는다.** ADR-0006이 이 실험을
  "저위험"으로 평가했던 전제 자체가 재검토 대상이다.

### 진행 방법 (블로킹 풀린 뒤)

1. 회귀가 해소돼 `Activate()`가 일반 프로세스(메모장 등)에서 다시 정상적으로 호출되는 걸
   먼저 확인한다.
2. `ImeStateTip.cpp`에 `ITfThreadMgrEventSink`(`OnSetFocus`) 구독 추가, `conhost.exe`에서
   실제로 발화하는지 최우선 검증(TSF 문서상 "애플리케이션 협조적" 신호로만 서술돼 미검증).
3. UI 쪽 포커스 신호 병합 우선순위 로직(TDD).
4. `cmd.exe` 반영 확인.
5. Excel `TF_IPPMF_ENABLEPROFILE` 실험 — 이번엔 **되돌릴 방법을 미리 정해두고**, 실험 전후
   레지스트리 스냅샷을 남겨서 만약 이번에도 회귀가 재현되면 정확히 뭐가 바뀌었는지 바로
   추적할 수 있게 한다.
6. 엔드투엔드 시나리오 재검증(메모장·Notepad++·mintty·패키지형 메모장·`cmd.exe`·Excel 전부).

### 결론에 포함할 것

각 항목의 성공/실패 + Excel 실험 결과에 따라 ADR-0006 자체를 갱신할지 여부.
