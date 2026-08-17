# 포커스 훅 미반응 앱 대응 — ADR-0007 구현 이후 발견된 공백

Label: wayfinder:map

## Destination

ADR-0007(포커스-트리거 INCONTEXT 인젝션) 프로덕션 구현(`src/ImeFocusHook/`) 이후, 인디케이터가
전혀 반응하지 않는 것으로 확인된 4개 앱(삼성증권 HTS, cmd.exe/conhost.exe, mintty, Windows 11
새 메모장) 각각의 원인을 규명하고 대응(수정 또는 대응 불가 판정)을 확정한다. `.scratch/ime-detection-strategy/`는
이미 destination에 도달해 닫혔으므로(ADR-0007 채택 + 프로덕션 검증 완료), 이 맵은 그 구현이
실사용 중 새로 드러낸 공백을 다루는 후속 맵이다.

## Notes

- 발견 경위: `fix/focus-hook-crash`(#2), `feat/exit-menu`(#3) 머지 후 실사용 중 발견. 크래시는
  아니고 조용히 무반응(로그도 에러도 없음).
- **공통 진단 1단계(모든 이슈 공통, 코드 수정 불필요)**: `ImeFocusHookDll.dll`이 해당 프로세스에
  실제로 로드되는지부터 확인한다(작업 관리자 세부 정보 탭 또는 `tasklist /m ImeFocusHookDll.dll`).
  인젝션 자체가 안 되는 경우와, 인젝션은 되는데 그 안의 TSF 초기화가 안 되는 경우는 원인이 완전히
  다르므로 반드시 먼저 갈라야 한다.
  - cmd.exe/conhost.exe: 지난 세션(2026-08-15)에 `conhost.exe`로 인젝션 성공을 이미 모듈
    목록으로 확인함 — "인젝션 성공" 그룹으로 이미 분류됨.
  - 나머지 3개 앱(HTS, mintty, 새 메모장)은 아직 미확인.
- **HTS는 원인이 다를 가능성이 높다**: 한국 증권사 HTS는 nProtect/TouchEn 계열 키보드 보안(안티
  키로거) 모듈을 흔히 동반하는데, 이런 모듈은 정확히 이 프로젝트가 하는 것과 같은 종류의 DLL
  인젝션/후킹을 감시·차단하도록 설계돼 있다. `.scratch/ime-detection-strategy/issues/01-security-software-investigation.md`에서
  다뤘던 보안 소프트웨어 간섭 문제와 같은 계열이지만, 그때는 MagicLine4NX/Citrix만 조사했고
  HTS 자체는 미조사였다.
- 새 메모장은 오늘 아침(트램폴린 창 방식 도입 전, `WinEventProc` 콜백에서 TSF 작업을 동기
  실행하던 버전)엔 정상 동작했다는 사용자 기록이 있음 — 동기→비동기 전환 이후 생긴 회귀일
  가능성이 유력한 가설.
- mintty(Cygwin/MSYS 계열 커스텀 창)와 새 메모장(WinUI3 기반으로 추정, 미확인)은 표준
  `GetMessage`/`DispatchMessage` 루프와 다르게 동작해, 트램폴린 창에 `PostMessage`한
  `WM_INIT_TSF`가 디스패치되지 않을 수 있다는 게 공유 가설.

## Decisions so far

- **2026-08-15, 처리 순서 확정(사용자 확정, 2026-08-16에 뒤집힘 — 아래 항목 참고)**: 신규
  기능(인디케이터 크기/위치 JSON 설정화)은 이 맵과 무관하게 가장 마지막으로 미룬다. 이 맵
  안에서는 "인젝션 성공 그룹부터, 인젝션 실패/불명 그룹(HTS 등)은 마지막" 순서로 진행한다 —
  HTS가 실사용 빈도는 가장 높지만, 원인 불확실성과 예상 조사 난이도가 가장 커서 "가치"가
  아니라 "오늘 안에 진전 가능성"을 기준으로 순서를 잡았다. 이슈 번호는 이 순서를 그대로
  반영한다(01 cmd/conhost → 02 새 메모장 → 03 mintty → 04 HTS).
- **2026-08-16, 우선순위를 실사용 가치 기준으로 재조정(사용자 확정)**: 새 메모장(issue 02)은
  세 번의 시도(스레드 재시도, 타이머 재시도, 컴파트먼트 전역 스코프)가 전부 실패로 끝났고,
  사용자가 Notepad를 많이 쓰지 않아 지원 가치가 낮다고 판단 — **issue 02를 `paused`로 내리고,
  그 시도들에 딸린 실험 코드는 전부 되돌렸다**(상세: issue 02 Resolution 절). 다음 순서는
  **issue 04(삼성증권 HTS)를 먼저** 진행한다 — 실사용 빈도가 가장 높다는 이유가 이번엔
  "오늘 안에 진전 가능성"보다 우선한다. issue 03(mintty)은 이전 세션(2026-08-15)에 이미
  `resolved`로 닫혀 있어 별도 조사 불필요.
- **2026-08-17, issue 04(HTS) 대응 불가로 확정(사용자 결정)**: `pdMain.exe`가
  `KOS_Service`(Kings Online Security)/`SafeTransactionSVC`(AhnLab Safe Transaction) 안티
  키로거 모듈에 의해 메모리 접근(`PROCESS_VM_READ`)이 차단된 것을 실측 확인 — map.md의
  사전 가설이 그대로 들어맞았다. 금융 앱의 보안 모듈을 우회하는 시도는 하지 않기로 결정,
  "알려진 한계"로 문서화만 하고 종료(상세: issue 04 Resolution 절). **이로써 이 맵의 4개
  이슈가 모두 처리됨**(01 resolved, 02 paused/deprioritized, 03 resolved, 04 resolved-대응불가)
  — "Out of scope" 게이트(신규 기능 착수 보류)가 해제됐다.

## Not yet specified

- 인디케이터 크기/위치 JSON 설정 기능(`docs/ui-spec.md`의 "빌드 시점 코드 상수로 관리, 런타임
  설정 파일 아님" 조항을 뒤집는 변경) — 이 맵의 범위 밖. 착수 시점에 별도
  `.scratch/<slug>/spec.md`로 관리할지 결정 필요.

## Out of scope

~~이 맵의 4개 이슈가 전부 resolved되기 전까지 신규 기능(설정 파일화) 착수는 이 맵의 범위 밖~~
(사용자 지시, 2026-08-15) — **2026-08-17, 4개 이슈 모두 처리 완료로 게이트 해제.** 신규 기능
(인디케이터 크기/위치 JSON 설정화) 착수 가능, "Not yet specified" 절 참고.
