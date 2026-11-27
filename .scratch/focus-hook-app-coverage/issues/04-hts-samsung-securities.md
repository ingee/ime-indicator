Type: task
Status: resolved (대응 불가 판정)

## Question

삼성증권 HTS에서 한/영 전환이 인디케이터에 반영되지 않는 원인은 무엇이며, 대응 가능한가?

### 배경

- 사용자가 자주 쓰는 앱 — 실사용 가치는 이 4개 중 가장 높지만, 원인이 다른 3개 앱과 다를
  가능성이 높아 순서상 마지막으로 미뤘다(`map.md` Decisions 참고).
- 한국 증권사 HTS는 nProtect/TouchEn 계열 키보드 보안(안티 키로거) 모듈을 흔히 동반한다 —
  이런 모듈은 이 프로젝트가 하는 것과 같은 종류의 DLL 인젝션/후킹을 감시·차단하도록 설계돼
  있어, 다른 세 앱의 "메시지 루프 특이성" 가설과는 다른 원인(인젝션 자체 차단)일 가능성이 있다.
- `.scratch/ime-detection-strategy/issues/01-security-software-investigation.md`에서 이미
  이 PC의 보안 소프트웨어(MagicLine4NX, Citrix)를 조사한 적 있으나 HTS 자체는 스코프 밖이었음
  — 그때 결론("관련성 낮음")이 HTS에도 적용되는지는 별개로 재확인 필요.
- 사용자에게 HTS에 실제로 어떤 보안 모듈이 설치돼 있는지 아직 확인 안 됨.

### 조사할 것

1. **가장 먼저**: `ImeFocusHookDll.dll`이 HTS 프로세스에 로드되는지 확인(작업 관리자 세부
   정보 또는 `tasklist /m`). 안 되면 로그 계측은 의미 없고 바로 2번으로 진행.
2. HTS 설치 시 같이 깔린 보안 모듈 확인(설치 프로그램 목록, 작업표시줄 트레이 아이콘) —
   nProtect/TouchEn/AhnLab Safe Transaction 등.
3. 인젝션이 되는데도 반응이 없다면 issue 01~03과 같은 방식(진단 로그)으로 이어서 조사.
4. 인젝션 자체가 막힌다면: 우회 가능한 방법이 있는지(예: 화이트리스트 등록, 다른 인젝션
   시점) 조사하되, 보안 소프트웨어를 우회하는 시도는 방어적 목적이라도 신중히 판단 —
   사용자와 상의 후 진행.

### 결론에 포함할 것

원인 판정(인젝션 차단 / 다른 원인) + 대응 가능 여부 + 대응 불가 시 "알려진 한계"로 문서화할지
여부.

## Resolution (2026-08-17)

**원인**: 인젝션 자체가 차단됨. HTS 실행 중(`pdMain.exe`, `C:\삼성증권\POPHTSN\pdMain.exe`)
`tasklist /m ImeFocusHookDll.dll`에 해당 프로세스가 나타나지 않았다. 관리자 권한 PowerShell로
`Get-Process -Id <pdMain PID>`를 직접 조회해도 `.Modules`가 0개, `.Path`가 빈 문자열로
반환됐다 — 예외 없이 조용히 실패하는 전형적인 `PROCESS_VM_READ` 차단 증상이다(같은 PID를
`Get-CimInstance Win32_Process`로 조회하면 `ExecutablePath`는 정상 반환됨 — WMI 경로는 이
권한을 요구하지 않으므로 프로세스 자체는 살아있고 조회 가능하나, 메모리 접근 계열 API만
막혀 있음이 대조 확인됨).

이 PC에서 실행 중인 서비스 확인 결과 `KOS_Service`(Kings Online Security)와
`SafeTransactionSVC`(AhnLab Safe Transaction)가 Running 상태로 떠 있었다 — 둘 다 국내
증권/은행 HTS에 흔히 동반되는 안티 키로거/메모리 보호 모듈이다. 우리 훅의 인젝션 경로
(`WriteProcessMemory`/`CreateRemoteThread` 계열, `WINEVENT_INCONTEXT`)가 요구하는 것과
정확히 같은 종류의 프로세스 메모리 접근 권한을 이 모듈들이 차단하고 있는 것으로 판단된다.
`.scratch/ime-detection-strategy/issues/01-security-software-investigation.md`에서 이미
확인했던 "보안 소프트웨어의 인젝션 차단" 패턴과 같은 계열이며, map.md의 사전 가설이 그대로
들어맞았다.

**대응 가능 여부**: 대응 불가로 판정, 우회 시도하지 않음(사용자 결정, 2026-08-17). 계좌
보안이 걸린 금융 앱의 안티 키로거 보호를 우회하는 것은 이 프로젝트의 목적(입력 상태 표시)에
비해 부적절하다고 판단했다.

**결론**: HTS는 "알려진 한계"로 문서화한다 — 이 앱에서는 인디케이터가 반응하지 않으며,
향후에도 고칠 계획이 없다.
