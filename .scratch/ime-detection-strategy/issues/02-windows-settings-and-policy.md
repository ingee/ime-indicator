Type: research
Status: open

## Question

Windows 설정 앱의 언어 옵션 고급 설정(예: "앱마다 다른 입력 방식 사용")이나 로컬 그룹 정책에
IME/키보드 관련 항목이 8/7 이후 바뀐 게 있는가?

### 배경

- 지금까지의 조사는 전부 `HKEY_USERS\<SID>\SOFTWARE\Microsoft\CTF\...`와
  `HKLM\SOFTWARE\Microsoft\CTF\...` 축에 집중돼 있었다 — 이 축은 CLSID 오염, `ENABLED`/`ACTIVE`
  플래그까지 상당히 소진됐다(map.md Notes 참고).
- 아직 안 본 축: 설정 앱 GUI 경로로만 노출되는 값(`InputMethodOverride` 등 다른 레지스트리
  위치), 그리고 `gpedit.msc`/`HKLM\SOFTWARE\Policies\Microsoft\...` 로컬 정책.
- 이 PC는 도메인/MDM 강제 통제 아래 있지 않다고 확인됐으므로([issue 01](01-security-software-investigation.md)
  배경 참고), 그룹 정책 쪽은 가능성이 상대적으로 낮지만, 로컬 정책은 일부 소프트웨어가 설치
  과정에서 직접 건드리는 경우도 있어 배제하지 않는다.

### 조사할 것

1. 설정 > 시간 및 언어 > 언어 및 지역 > 한국어 > 언어 옵션 > 키보드 고급 설정에서 "앱마다
   다른 입력 방식 사용" 등의 체크 상태 확인, 관련 레지스트리 값(`HKCU\Control Panel\International\User Profile`
   의 `InputMethodOverride` 등) 조회.
2. `gpedit.msc` 또는 `HKLM\SOFTWARE\Policies\Microsoft\...` 아래 IME/키보드 관련 정책이
   설정돼 있는지 확인.
3. 8/7 이전에 이 설정을 사용자가 직접 건드린 기억이 있는지 확인(레지스트리 타임스탬프만으로는
   변경 시점을 특정하기 어려우므로 사용자 기억과 교차 검증 필요).

### 결론에 포함할 것

관련 있음/없음 판정 + 근거.
