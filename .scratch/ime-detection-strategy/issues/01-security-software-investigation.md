Type: research
Status: resolved

## Question

이 PC에 설치된 키보드 보안/원격접속 소프트웨어(`MagicLine4NX.exe`, Citrix 관련 프로세스
`concentr.exe`/`Receiver.exe`/`wfcrun32.exe`/`SelfServicePlugin.exe` 등)가 우리 TIP의
`Activate()` 미호출 회귀와 관련이 있는가?

### 배경

- 2026-08-14 세션 중 새로 만든 테스트 계정(`ime-test`)의 프로세스 목록을 조회하다가 우연히
  발견함 — 지금까지 이 축은 전혀 조사한 적이 없다.
- 이런 종류의 "키보드 보안" 모듈은 흔히 키로거 방지 목적으로 IME/TSF 후킹 자체를 감시·제한하는
  기능을 갖고 있다 — 우리 TIP이 하려는 것(다른 프로세스의 키보드/IME 상태 관찰)과 정확히 같은
  영역을 건드리는 소프트웨어다.
- 이 PC는 회사 업무도 보는 **개인 소유** PC다(사용자 확인, 2026-08-14) — 강력한 중앙(도메인/MDM)
  통제 아래 있지는 않다. 그룹 정책보다는 **이 소프트웨어 자체의 자동 업데이트로 동작이 바뀌었을
  가능성**에 무게를 둔다.
- 8/7(정상 동작) → 8/8 밤(회귀 시작) 사이에 이 소프트웨어들이 업데이트됐는지 대조가 핵심.

### 조사할 것

1. 설치된 프로그램 목록에서 이 소프트웨어들의 설치일자/버전 조회
   (`HKLM:\SOFTWARE\...\Uninstall\*`, `HKLM:\SOFTWARE\WOW6432Node\...\Uninstall\*`).
2. Windows 이벤트 로그(Application, 또는 각 프로그램 자체 업데이트 로그)에서 8/7~8/8 사이
   업데이트/재설치 이벤트가 있는지 확인.
3. (신중하게, 사용자 동의 하에) 이 소프트웨어들을 일시적으로 종료한 상태에서 Notepad를 새로
   띄워 `Activate()`가 되는지 재테스트 — 업무용 프로그램이므로 되돌릴 방법을 미리 정하고
   진행할 것. 삭제/재설치는 하지 않는다.
4. 이 소프트웨어들이 TSF/CTF 관련 레지스트리 키(`CTF\TIP\...`, `CTF\Assemblies\...`)에 직접
   쓰기 권한을 요구하거나 필터링 드라이버를 설치하는지 문서/공개 자료로 조사.

### 결론에 포함할 것

관련 있음/없음 판정 + 근거. 관련 있다면 이 소프트웨어를 우회하거나 함께 동작할 방법이 있는지도
포함.

## Answer

**판정: 관련성 낮음.** 8/7→8/8 회귀를 이 소프트웨어들의 변화로 설명할 직접 증거는 못 찾았다.
실측 종료 재테스트(조사할 것 3번)까지는 안 갔다 — 아래 근거로 우선순위가 낮다고 판단해 사용자와
상의 후 생략.

- **"업데이트가 원인" 가설은 기각.** MagicLine4NX·Citrix Workspace 전 제품군의 바이너리
  수정일이 전부 2023년(최신도 2024-01-19, `MagicLine4NX_Uninstall.exe`)이고, 8/6~8/9 사이
  Windows 이벤트 로그(Application의 `MsiInstaller`, System의 SCM)에 두 제품 언급이 전혀 없다.
  파일 교체를 동반한 업데이트/재설치는 이 구간에 없었다.
  - 참고로 이 구간 유일한 MSI 이벤트는 8/7 오전 `Microsoft.NET.Workloads.10.0.300` SDK
    설치였다 — 회귀 시점과 겹치지만 보안 소프트웨어와 무관해 별개로만 기록.
- **Citrix App Protection**(`entryprotectdrv`/`epinject6`/`epusbfilter`, 파일:
  `entryprotect.sys`/`epinject.sys`/`epusbfilter.sys`, 전부 상시 `Running`)은 실제로 커널
  레벨 anti-keylogging 드라이버였다(Citrix 공식 문서로 확인). 다만 공식 문서에 **"보호 대상
  창(protected window)이 포커스를 가졌을 때만 활성화"**된다고 명시돼 있다. 회귀 재현에 쓴
  대상(Notepad, cmd.exe, Excel)은 Citrix로 게시된 앱이 아니므로, 이 스코프 설명이 맞다면
  전역적으로 TIP `Activate()`를 막을 이유가 약하다. 완전히 배제는 못 하지만(정책이 문서와 다르게
  동작할 가능성, 로컬 레지스트리에서 활성화 플래그를 못 찾음 — `HKLM:\SOFTWARE\Citrix` 재귀
  조회는 트리가 너무 깊어 PowerShell이 StackOverflow로 죽어서 서비스 키 직접 조회로만 확인)
  유력 후보는 아니다.
- **MagicLine4NX**는 공동인증서 로그인용 인증 미들웨어(Dreamsecurity)다. 설치된 버전은
  `1.0.0.29`로 취약점이 보고된 `1.0.0.26` 이하보다 최신. 이 시스템의 커널 드라이버 목록에
  Dreamsecurity 관련 드라이버가 없었고(Citrix만 발견), 키보드/TSF 후킹 관련 공개 자료도 못
  찾았다 — 관련성 낮음.
- **TSF/CTF 레지스트리 키 직접 조사(조사할 것 4번)는 미완료.** 위 스코프 정황상 우선순위가
  낮아 서비스 키 수준 확인으로 갈음했다. 나중에 이슈 02나 회귀 원인이 여전히 안 풀리면
  재검토 후보로 남겨둔다.

결론적으로 이 축은 닫는다. 다음은 [issue 02](02-windows-settings-and-policy.md)(Windows
설정/로컬 정책)로 넘어간다.
