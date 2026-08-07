---
status: superseded by ADR-0004
---

# TIP으로 등록해 다른 프로세스의 TSF 상태를 감지하고, 그 DLL은 감지 기능만 담당한다

독립 EXE에서 `TF_CreateThreadMgr`로 만든 `ITfThreadMgr`에 `ITfThreadMgrEventSink`를
구독해봤지만, 다른 프로세스(노트패드 등)에서 한/영을 바꿔도 콜백이 전혀 오지 않는 것을
실측으로 확인했다. TSF의 스레드 매니저·컴파트먼트 객체는 그 인스턴스를 만든 프로세스
안에서만 유효하며, `AttachThreadInput`도 Win32 포커스만 공유할 뿐 TSF 내부 상태는
공유하지 않는다. 다른 프로세스의 실제 TSF 상태를 관찰하려면 그 프로세스 안에 코드가
실제로 로드되어야 하고, TSF에서 이를 가능하게 하는 공식 메커니즘이 TIP(Text Input
Processor) 등록이다.

대안으로 전역 키보드 훅으로 `VK_HANGUL` 토글을 추적하는 방법도 검토했지만, 이는 AHK
프로토타입이 이미 겪은 "메아리" 문제(앱이 키 입력 없이 프로그래밍적으로 상태를 바꾸면
감지 못함)를 구조적으로 그대로 안고 가게 되어 기각했다 — 이 프로젝트를 새로 만드는
이유 자체가 그 부정확함을 없애는 것이었다(CLAUDE.md 2절).

이에 TIP으로 정식 등록하기로 한다. 다만 TIP DLL은 텍스트 입력을 다루는 **모든
프로세스**에 로드되므로, 거기서 나는 버그나 성능 문제는 우리 앱이 아니라 시스템 전반의
다른 앱들에 영향을 준다. 그래서 이 DLL의 책임을 의도적으로 최소화한다 — 오직
`GUID_COMPARTMENT_KEYBOARD_OPENCLOSE` 상태를 감지해 UI 프로세스에 전달하는 것까지만
하고, 어떤 색으로 표시할지·마지막 상태 유지 규칙·클릭 반전 같은 로직은 전부 기존
WinForms UI 프로세스 쪽에 남긴다.

## Considered Options

- 전역 키보드 훅으로 `VK_HANGUL` 추적 — 배포는 단순(단일 EXE, 관리자 권한 불필요)하지만
  AHK와 같은 근본적 부정확함이 남아 기각.

## Consequences

- 배포 방식이 "설치 시 관리자 권한 1회 필요 + 레지스트리 등록"으로 바뀐다 — ADR-0001과
  CLAUDE.md 7절(single-file exe, 설치 불필요)을 다시 손봐야 한다.
- TIP DLL과 UI EXE 사이의 IPC 방식은 별도로 결정해야 한다.
