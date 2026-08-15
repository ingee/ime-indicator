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

## 4. 현재 상태 — 포커스-트리거 INCONTEXT 주입 채택 (2026-08-15)

TIP 기반 접근(ADR-0001~0006)은 2026-08-07까지는 정상 동작을 확인한 유력한 후보였으나,
2026-08-08 이후 원인을 알 수 없는 회귀로 대부분의 프로세스에서 TIP `Activate()`가 더 이상
호출되지 않게 됐다(원인 미해결, `.scratch/ime-detection-strategy/issues/02`). 세 세션에 걸친
원인 조사에도 근본 원인을 찾지 못해, **그 자동 로드 진입점 자체에 대한 의존을 포기**하고 대안을
실측했다.

**[ADR-0007](docs/adr/0007-focus-triggered-incontext-injection.md)에 따라, 포커스가 바뀔
때마다 `SetWinEventHook(EVENT_SYSTEM_FOREGROUND, ..., WINEVENT_INCONTEXT)`로 그 순간 포커스를
얻은 프로세스 안에 직접 코드를 주입하고, 그 안에서 우리가 직접 `CoCreateInstance(CLSID_TF_ThreadMgr)`
→ `Activate()` → Compartment 구독까지 거는 방식을 새로운 핵심 감지 메커니즘으로 채택한다.**
5단계 전부 실측 검증됨(`.scratch/ime-detection-strategy/issues/07-focus-triggered-incontext-injection.md`):
인프로세스 주입 확정적 성공, 드롭 없는 신뢰성, TSF가 이 진입점을 정상 참가자로 받아들여 8/7
이전과 동일하게 동작, x64+x86 훅 병행으로 32비트 Office(TIP이 한 번도 못 커버했던 범위)까지
지원. ADR-0003의 "TIP 자동 로드" 전제와 issue 02의 회귀 원인은 더 이상 풀어야 할 문제가 아니다.

프로토타입은 `prototype/langbar-observation-poc-throwaway` 브랜치의 `LangBarPoc10`/
`LangBarPoc10-x86`/`LangBarPoc11`에 있고, 실제 프로덕션 구현(`src/`)은 아직 이 프로토타입을
반영하지 않은 채로 남아 있다 — 다음 세션의 주요 작업. 남은 위험(스레드 마샬링, 크래시 블라스트
반경, AV/EDR 오탐, 포커스별 재구독)은 ADR-0007 Consequences 절 참고.

진행 상황과 다음 시도는 아래 Agent skills의 Issue tracker 절이 가리키는 wayfinder 맵을
따른다. 구현 설계 자체는 `docs/adr/0002`~`0007`과 `src/ImeIndicatorTip/`, `src/ImeIndicator/`
코드가 최신 출처다(코드가 실제로 하는 일과 이 문서가 어긋나면 코드를 신뢰할 것 — 단, `src/`는
아직 새 아키텍처를 반영하지 않았으므로 새 진입점 관련해서는 프로토타입 브랜치와 ADR-0007이
우선한다).

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
