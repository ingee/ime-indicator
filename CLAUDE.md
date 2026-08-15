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
`LangBarPoc10-x86`/`LangBarPoc11`에 있다. **실제 프로덕션 구현도 같은 날 `src/ImeFocusHook/`
(훅 DLL + 로더 EXE, x64+x86)로 완료됐고, 사용자가 Excel 포함 여러 앱에서 한/영 전환을 직접
검증했다**(ADR-0007 Update 절). 옛 TIP 프로젝트 `src/ImeIndicatorTip/`는 완전히 대체돼
삭제했고, 그 레지스트리 등록(HKLM CLSID, CTF TIP 프로필/카테고리, HKCU Enable 플래그)도
정리했다. 남은 위험(스레드 마샬링, 크래시 블라스트 반경, AV/EDR 오탐, 포커스별 재구독,
`cmd.exe`/`conhost.exe` 재검증)은 ADR-0007 Consequences 절과 map.md "Not yet specified"
참고 — 전부 다음 작업으로 남아 있다.

**개발 중 자기 자신의 터미널을 잠그지 않으려면**: 이 훅은 시스템 전역이라, `ImeIndicator.exe`를
띄운 채로 재현 테스트를 하다가 결과를 공유하려고 개발용 터미널(mintty 등, Claude Code 세션을
호스팅하는 프로세스 포함)로 포커스를 돌리면 그 터미널 자신이 주입 대상이 되어
`ImeFocusHookDll.dll`이 pin된다 — 다음 재빌드 때 그 터미널을 재시작하기 전까지 `release/`
폴더로 복사가 막힌다. **트레이 "종료" 메뉴를 누르면 로더 프로세스가 죽으면서 훅도 즉시
해제되므로, 개발용 터미널로 돌아오기 전에 먼저 "종료"부터 누른다**(순서: `ImeIndicator.exe`
실행 → 테스트 대상 앱에서 재현 → 트레이 "종료" → 그다음 터미널로 복귀해 결과 공유; mintty에서
실행했어도 상관 없다). 상세 배경은 ADR-0007 Update(2026-08-16) 참고.

**빌드는 `build`, 배포는 `release`로 분리한다**: `scripts/build.ps1`은 컴파일만 하고
`release/`는 건드리지 않는다 — 세션 중 반복 테스트는 `src/ImeIndicator/bin/Release/
net8.0-windows/ImeIndicator.exe`(빌드 산출물이 이미 자체 실행 가능한 상태로 여기 있다)를
직접 실행한다. `release/`는 세션을 마무리하며 그 시점의 빌드가 지금 `release/`에 있는
버전보다 완성도가 높다고 판단될 때만 `scripts/release.ps1`(내부적으로 `build.ps1`을 다시
호출한 뒤 `release/`를 새로 만듦)로 승격한다 — 세션 도중의 실험적 빌드가 실제 배포판에
섞이는 것을 막기 위함이다.

진행 상황과 다음 시도는 아래 Agent skills의 Issue tracker 절이 가리키는 wayfinder 맵을
따른다. 구현 설계 자체는 `docs/adr/0002`~`0007`과 `src/ImeFocusHook/`, `src/ImeIndicator/`
코드가 최신 출처다(코드가 실제로 하는 일과 이 문서가 어긋나면 코드를 신뢰할 것).

## 5. 참고

- [UI 스펙](docs/ui-spec.md) — 화면 표시값, 클릭 반전 등 상호작용, 배포·시작프로그램 등록
  방식을 정리한 구현 참고 문서. UI나 배포 관련 코드를 만질 때만 참조한다.
- [참고 자료](docs/prototype-reference.md) — 위 값들의 근거가 된 AHK 프로토타입과 트레이
  아이콘 디자인 이미지.

## Agent skills

### Issue tracker

`.scratch/`의 로컬 마크다운 — `docs/agents/issue-tracker.md` 참고. 현재 진행 중인 조사는
[`focus-hook-app-coverage` map](.scratch/focus-hook-app-coverage/map.md)(ADR-0007 구현 이후
반응하지 않는 것으로 발견된 4개 앱 대응). 이전 아키텍처 재검토
[`ime-detection-strategy` map](.scratch/ime-detection-strategy/map.md)은 destination에
도달해 참고용으로만 남아 있다.

### Domain docs

`CONTEXT.md` + `docs/adr/` — `docs/agents/domain.md` 참고.
