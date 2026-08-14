Type: task
Status: open

## Question

완전히 새로운 CLSID/프로필 GUID를 만들어, 진단 도구(`ActivateProfile`/`DeactivateProfile`)를
단 한 번도 호출한 적 없는 상태로 `regsvr32`(→`AddLanguageProfile`)만으로 등록하면, `ime-test`
계정처럼 아무 이력도 없는 계정에서 선택 없이도 `Activate()`가 자동으로 되는가?

### 배경

- 2026-08-14 세션에 이 실험의 가치를 검토했다 — 결론은 **기대값이 낮다**는 쪽이었다:
  - `HKLM`의 `LanguageProfile` 키에는 `Enable` 관련 값 자체가 없다(등록 코드가 아예 안 씀).
  - `Enable` 오버라이드는 지금까지 전부 계정별 `HKCU`에서만 발견됐지, CLSID에 안 묶인 전역
    위치에서는 한 번도 발견 못 함.
  - 즉 "CLSID 자체에 진단 도구 호출 이력이 흔적을 남긴다"는 가설은 반증할 적극적 근거가 없고,
    "아직 못 찾았을 뿐"이라는 소극적 근거만 있다.
  - 반면 실험 비용(새 GUID 생성, 재빌드, 관리자 권한 `regsvr32` 재등록)은 꽤 든다.
- 그래서 최우선 순위에서 뺐고, [issue 01](01-security-software-investigation.md)/
  [issue 02](02-windows-settings-and-policy.md) 같은 다른 후보들이 전부 막히면 시도할
  **잔여 후보**로만 남겨둔다.

### 진행 방법 (실제로 하게 되면)

1. 새 CLSID/프로필 GUID 생성, `Guids.h` 갱신, 재빌드.
2. `regsvr32`로만 등록 — 진단 도구(`_diag_check_enabled.cpp`)는 절대 실행하지 않는다.
3. `ime-test` 계정(또는 그때 시점에 이력이 없는 다른 깨끗한 계정)에서 새 프로세스를 띄워,
   선택하지 않은 채로 `Activate()`가 자동으로 되는지 확인.

### 결론에 포함할 것

되는지/안 되는지 + (되면) CLSID에 실제로 어떤 흔적이 남았던 건지 추적, (안 되면) CLSID 이력
가설 완전 폐기.
