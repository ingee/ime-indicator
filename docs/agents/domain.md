# 도메인 문서

엔지니어링 스킬들이 코드베이스를 탐색할 때 이 저장소의 도메인 문서를 어떻게 참고해야 하는지 정리한다.

## 탐색 전에 먼저 읽을 것

- 저장소 루트의 **`CONTEXT.md`**, 또는
- 저장소 루트에 **`CONTEXT-MAP.md`**가 있다면 그 파일 — 컨텍스트별 `CONTEXT.md` 위치를 가리킨다. 다루려는 주제와 관련된 것들을 각각 읽는다.
- **`docs/adr/`** — 지금 작업하려는 영역과 관련된 ADR을 읽는다. 멀티컨텍스트 저장소라면 `src/<context>/docs/adr/`의 컨텍스트별 결정도 함께 확인한다.

이 파일들이 존재하지 않으면 **조용히 넘어간다**. 없다는 사실을 굳이 지적하거나, 미리 만들자고 제안하지 않는다. `/domain-modeling` 스킬(`/grill-with-docs`, `/improve-codebase-architecture`를 통해 도달)이 용어나 결정이 실제로 확정되는 시점에 필요할 때만 만든다.

## 파일 구조

단일 컨텍스트 저장소 (대부분의 저장소):

```
/
├── CONTEXT.md
├── docs/adr/
│   ├── 0001-event-sourced-orders.md
│   └── 0002-postgres-for-write-model.md
└── src/
```

멀티 컨텍스트 저장소 (루트에 `CONTEXT-MAP.md`가 있는 경우):

```
/
├── CONTEXT-MAP.md
├── docs/adr/                          ← 시스템 전역 결정
└── src/
    ├── ordering/
    │   ├── CONTEXT.md
    │   └── docs/adr/                  ← 컨텍스트별 결정
    └── billing/
        ├── CONTEXT.md
        └── docs/adr/
```

## 용어집의 어휘를 사용할 것

이슈 제목, 리팩터 제안, 가설, 테스트 이름 등 출력물에 도메인 개념을 명명할 때는 `CONTEXT.md`에 정의된 용어를 사용한다. 용어집이 명시적으로 피하는 동의어로 흘러가지 않는다.

필요한 개념이 아직 용어집에 없다면 그건 신호다 — 프로젝트가 쓰지 않는 말을 지어내고 있는 것이거나(재고할 것), 아니면 진짜 빈틈이 있는 것이다(`/domain-modeling`을 위해 기록해 둘 것).

## ADR 충돌은 반드시 알릴 것

출력물이 기존 ADR과 모순된다면 조용히 덮어쓰지 말고 명시적으로 드러낸다:

> _ADR-0007(event-sourced orders)과 모순됨 — 다만 …한 이유로 재검토할 가치가 있음_
