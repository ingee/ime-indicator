# CLAUDE.md — Windows IME 상태 인디케이터

## 1. 개요

Windows에서 한글/영문 입력 상태(IME)를 항상 표시해 주는 상시 실행 프로그램이다. 듀얼 모니터
환경에서 입력 상태를 인지하지 못해 오타가 반복되는 문제를, 각 모니터 구석에 항상 떠 있는
인디케이터로 해결한다.

## 2. 배경

레거시 IMM API(`ImmGetContext`)는 이 PC의 모든 앱에서 `himc=0`을 반환한다. 입력기가
**TSF로만 동작**하고 IMM 호환 계층은 사실상 작동하지 않는다는 뜻이다. `VK_HANGUL` 전역
토글값도 시도해봤지만, TSF가 관리하는 진짜 상태의 파생값일 뿐이라 어긋남이 계속됐다.
**결론: TSF의 진짜 상태 저장소(Compartment)를 폴링하지 않고 이벤트 콜백
(`ITfSource`/`AdviseSink`)으로 구독해야 한다.** 이 결론은 AHK 프로토타입을 검증하며 얻은
교훈이므로 다시 검증할 필요는 없다([참고 자료](docs/prototype-reference.md)).

## 3. 목표 / 비목표

**목표** (2026-08-14 재확정 — 구현 수단을 목표에 끼워넣지 않는다)

1. 시스템의 한/영 입력 상태를 파악한다
2. 시스템의 한/영 입력 상태 변화 이벤트를 획득한다
3. 시스템의 한/영 입력 상태를 모든 모니터에 표시한다

세 목표 모두 **오차 없이**, **폴링이 아닌 이벤트 기반**으로 달성한다.

**비목표**

- 한글 외 다른 언어 지원 — 필요하면 나중에 확장한다.
- ~~키 입력을 한/영으로 전환하는 실제 IME 역할은 맡지 않는다 (표시 전용)~~ —
  **재검토 중, 확정 아님.** 자세한 배경과 진행 상황은
  [`ime-detection-strategy` issue 03](.scratch/ime-detection-strategy/issues/03-selection-required-nongoal-conflict.md).

## 4. 현재 상태 — 아키텍처 재검토 중 (2026-08-14)

TIP 기반 접근(ADR-0001~0006)은 2026-08-07까지는 정상 동작을 확인한 유력한 후보였다. 그런데
2026-08-08 이후 원인을 알 수 없는 회귀가 생겼다(TIP `Activate()`가 대부분 프로세스에서 호출
안 됨). 조사 과정에서 ADR-0003/ADR-0005의 핵심 전제("선택 안 된 관찰자 TIP도 자동
로드된다")마저 실측과 모순된다는 사실도 드러났다 — **이 두 ADR을 그대로 신뢰하지 말 것.**

진행 상황과 다음 시도는 아래 Agent skills의 Issue tracker 절이 가리키는 wayfinder 맵을
따른다. 구현 설계 자체는 `docs/adr/0002`~`0006`과 `src/ImeIndicatorTip/`, `src/ImeIndicator/`
코드가 최신 출처다(코드가 실제로 하는 일과 이 문서가 어긋나면 코드를 신뢰할 것).

## 5. 참고

- [UI 스펙](docs/ui-spec.md) — 화면 표시값, 클릭 반전 등 상호작용, 배포·시작프로그램 등록
  방식을 정리한 구현 참고 문서. UI나 배포 관련 코드를 만질 때만 참조한다.
- [참고 자료](docs/prototype-reference.md) — 위 값들의 근거가 된 AHK 프로토타입과 트레이
  아이콘 디자인 이미지.

## Agent skills

### Issue tracker

`.scratch/`의 로컬 마크다운 — `docs/agents/issue-tracker.md` 참고. 현재 진행 중인 아키텍처
재검토는 [`ime-detection-strategy` map](.scratch/ime-detection-strategy/map.md).

### Domain docs

`CONTEXT.md` + `docs/adr/` — `docs/agents/domain.md` 참고.
