# TSF 포커스 신호 병행 채택 + Office TIP 활성화 저위험 실험

ADR-0005의 PID 매칭 방식은 `cmd.exe`(보이는 창의 PID가 실제 TIP이 로드되는 프로세스인
`conhost.exe`와 다름)와 Excel(TIP의 `Activate()` 자체가 호출되지 않음)에서 실측으로 깨지는
게 확인됐다(CLAUDE.md 3절 "알려진 제약", `CLAUDE_worklist.md` 3절 미완료 항목). 이 두 문제를
풀기 위해 wayfinder 맵(`.scratch/ime-focus-accuracy/`)으로 원인을 조사했다.

## cmd.exe/conhost.exe — TSF 네이티브 포커스 신호 병행 채택

리서치(`.scratch/ime-focus-accuracy/issues/01-tsf-native-focus-signal.md`,
`docs/research/tsf-native-focus-signal.md`, 브랜치 `research/tsf-native-focus-signal`)로
확인된 사실:

- `conhost.exe`에서 보이는 콘솔 창의 소유 PID가 `cmd.exe`로 보고되는 것은, `conhost.exe`가
  콘솔 창의 소유 PID/TID를 커널 레벨에서 클라이언트 프로세스로 바꿔치기하는 문서화된
  오퍼레이션(`ConsoleControl`의 `ConsoleSetWindowOwner`) 때문일 가능성이 높다. 이건
  `GetForegroundWindow`/`GetWindowThreadProcessId` 같은 외부 조회 API에만 영향을 준다 —
  TIP이 자기 프로세스 안에서 `GetCurrentProcessId()`로 직접 얻는 PID는 이 바꿔치기와
  무관하다.
- `ITfThreadMgrEventSink::OnSetFocus`는 TIP이 이미 `Activate()`에서 받는 스레드 스코프
  `ITfThreadMgr`에 그대로 걸 수 있는, 폴링이 필요 없는 콜백이다(기존 컴파트먼트 구독과
  동일한 `ITfSource::AdviseSink` 패턴).

**결정**: TIP이 컴파트먼트 상태 보고에 더해 `OnSetFocus`도 구독해 "이 PID가 TSF 포커스를
얻음/잃음"을 타임스탬프와 함께 IPC로 보고한다. UI는 `SetWinEventHook` 기반 포그라운드 PID
추적을 완전히 대체하지 않고 그대로 유지하되, 그 PID가 상태 테이블에 없거나 더 최신 "포커스
획득" 보고가 있는 PID가 있으면 그쪽을 우선한다. 완전 대체하지 않는 이유는 Excel처럼 TIP의
`Activate()` 자체가 없는 프로세스에서는 이 신호도 존재하지 않기 때문이다 —
`SetWinEventHook`이 그 공백의 최후 방어선으로 계속 필요하다.

`conhost.exe`에서 `OnSetFocus`가 실제로 발화하는지는 문서만으로 확정하지 못했다(TSF 문서상
"애플리케이션 협조적" 신호로 서술됨) — 구현 착수 시 최우선으로 실측 검증한다.

## Excel — 완전 해결책은 비목표와 충돌, 저위험 실험만 시도

리서치(`.scratch/ime-focus-accuracy/issues/02-office-tip-activation-gap.md`,
`docs/research/office-tip-activation-gap.md`, 브랜치 `research/office-tip-activation-gap`)로
확인된 사실:

- Excel 셀 에디터가 왜 관찰자 TIP을 로드하지 않는지 근본 원인은 확정할 수 없었다(Excel
  내부 구현이 비공개). 가장 유력한 가설(중간 신뢰도, 추론): TSF-unaware 컨트롤이라 TSF의
  "Transitory Context" 경로를 타고, 그 경로는 선택된 TIP 하나만 필요로 한다.
- 확실하게 고치는 방법(`ActivateProfile`을 `TF_IPPMF_FORPROCESS`/`FORSESSION`으로 호출해
  실제 "선택된" 프로필이 되기)은 존재하지만, 그러면 이 TIP이 실제 키 입력을 가로채 처리하게
  되어 CLAUDE.md 3절 비목표("IME 자체의 동작을 변경하거나 가로채는 기능 — 표시 전용")를
  정면으로 어긴다.

**결정**: 비목표를 어기지 않는 절충안인 `TF_IPPMF_ENABLEPROFILE`("선택 전환 없이 설치된
키보드 목록에만 등록")을 저위험 실험으로 우선 시도한다. 성공 여부를 보장하는 1차 자료는
없어 실측이 필요하다. **이 실험이 실패하더라도 자동으로 "알려진 제약"으로 문서화하고
넘어가지 않는다** — 다시 사용자와 상의해 다음 방안을 정한다. (2026-08-07 세션에서 "알려진
제약으로 문서화하고 넘어가려던 것"이 명시적으로 거부된 전례가 있다 — "모든 앱에서 정확히
표시"는 이 프로젝트의 타협 불가능한 핵심 목표, CLAUDE.md 3절.)

## Consequences

- ADR-0005의 핵심 결정(스레드 스코프 컴파트먼트 구독, PID 매칭)은 그대로 유지된다 — 이
  ADR은 그 위에 포커스 신호 하나를 추가하는 것이지 대체가 아니다.
- CLAUDE.md 4절을 이 결정에 맞게 갱신한다.
- `CLAUDE_worklist.md` 3절의 미완료 항목("엔드투엔드 수동 시나리오 검증")을 이 결정을
  구현하는 구체적 하위 항목들로 재작성한다.
- wayfinder 맵(`.scratch/ime-focus-accuracy/`)은 이 ADR로 destination에 도달했으므로 이후
  세션에서는 참고용으로만 남는다.
