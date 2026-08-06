# 이슈 트래커: 로컬 마크다운

이 저장소의 이슈와 스펙(PRD라고 부르기도 함)은 `.scratch/` 아래 마크다운 파일로 존재한다.

## 규칙

- 기능 하나당 디렉터리 하나: `.scratch/<feature-slug>/`
- 스펙 파일: `.scratch/<feature-slug>/spec.md`
- 구현 이슈는 티켓 하나당 파일 하나로 `.scratch/<feature-slug>/issues/<NN>-<slug>.md`에 `01`부터 번호를 매겨 작성한다 — 여러 티켓을 한 파일에 몰아 쓰지 않는다
- 트리아지 상태는 각 이슈 파일 상단의 `Status:` 줄에 기록한다 (역할 문자열은 `triage-labels.md` 참고)
- 코멘트/대화 이력은 파일 맨 아래 `## Comments` 제목 아래에 이어 붙인다

## 스킬이 "이슈 트래커에 게시하라"고 할 때

`.scratch/<feature-slug>/` 아래에 새 파일을 만든다 (디렉터리가 없으면 새로 생성).

## 스킬이 "해당 티켓을 가져오라"고 할 때

참조된 경로의 파일을 읽는다. 보통 사용자가 경로나 이슈 번호를 직접 알려준다.

## Wayfinding 동작

`/wayfinder`에서 사용한다. **map**은 티켓별 **child** 파일 하나씩을 가리키는 파일이다.

- **Map**: `.scratch/<effort>/map.md` — Notes / Decisions-so-far / Fog 본문.
- **Child 티켓**: `.scratch/<effort>/issues/NN-<slug>.md`, `01`부터 번호를 매기고 본문에 질문을 적는다. `Type:` 줄에 티켓 종류(`research`/`prototype`/`grilling`/`task`)를, `Status:` 줄에 `claimed`/`resolved`를 기록한다.
- **Blocking**: 상단 근처에 `Blocked by: NN, NN` 줄을 둔다. 나열된 파일이 모두 `resolved`가 되면 그 티켓의 블로킹이 풀린다.
- **Frontier**: `.scratch/<effort>/issues/`를 훑어 open이면서 unblocked, unclaimed인 파일을 찾는다. 번호가 낮은 것이 우선이다.
- **Claim**: 작업 시작 전에 `Status: claimed`로 설정하고 저장한다.
- **Resolve**: `## Answer` 제목 아래에 답을 덧붙이고 `Status: resolved`로 설정한 뒤, map의 Decisions-so-far에 맥락 포인터(요약 + 링크)를 `map.md`에 덧붙인다.
