# Office(Excel)에서 우리 TIP의 Activate()가 안 불리는 원인

Type: research
Status: resolved

## Question

우리 TIP이 왜 Excel(Office) 셀 편집 중 `Activate()`조차 호출되지 않는지 — 반면 실제 활성 IME
(Microsoft IME)는 정상적으로 Excel에서 작동하고, Windows 자체 입력 표시기도 Excel 셀 편집 중
정확하게 갱신된다(2026-08-08 사용자 실측 확인, Office 2010 Excel 기준 — 한/영 전환만 해도, 셀을
선택하고 문자를 입력할 때도 입력 표시기가 정확히 전환됨).

### 배경

- 이 사실은 Excel의 셀 편집기가 TSF 바깥의 완전히 별개 입력 체계를 쓰는 게 아니라, TSF를 정상적으로
  타고 있다는 강력한 증거다(Windows 표시기가 다른 경로로 상태를 알 방법이 없으므로 — 조사에서
  확인된 대로 그 표시기의 유일한 메커니즘은 "대상 프로세스 안의 in-process TSF 클라이언트가
  push"하는 것뿐).
- `ITfCategoryMgr::RegisterCategory(GUID_TFCAT_TIP_KEYBOARD)` 카테고리 등록을 추가해봤지만 결과가
  같았음(`Registration.cpp`, 2026-08-07 커밋) — 단순 카테고리 등록 누락이 아닌 것으로 보인다.
- 우리 TIP은 "관찰만 하는 보조 TIP"이고, 실제 선택된 활성 IME가 아니다 — 이 차이가 원인일 가능성이
  있다.

### 조사할 것

1. TSF가 "어떤 TIP을 특정 프로세스/스레드에 로드할지" 결정하는 조건은 무엇인가 — 단순히
   `GUID_TFCAT_TIP_KEYBOARD` 카테고리로 등록돼 있으면 충분한지(다른 앱들에서는 충분했음), 아니면
   Office 계열 앱이 추가로 요구하는 카테고리/능력(`GUID_TFCAT_TIPCAP_*` 등)이 있는지.
2. `ITfInputProcessorProfileMgr`/`ActivateProfile` 경로를 통해 실제 "선택 가능한 언어 프로필"로
   등록해야만 Office에서 로드되는지 — 즉 관찰자 TIP으로 남는 한 근본적으로 불가능한 제약인지,
   아니면 카테고리 추가만으로 해결 가능한지.
3. Office(엑셀 등)의 그리드/셀 인플레이스 에디터가 TSF TIP 목록을 필터링하는 특별한 방식이
   문서화돼 있는지(공식 문서, SDK 헤더 주석, 신뢰할 만한 TSF/Office 통합 해설).
4. 만약 "선택 가능한 활성 IME로 등록"이 유일한 해법이라면, 이게 우리 프로젝트 목표(표시 전용, IME
   자체 동작을 변경·가로채지 않음 — CLAUDE.md 3절 비목표)와 충돌하지 않는지도 짚을 것 — 관찰만
   하면서 선택 가능 프로필로만 등록하는 절충이 가능한지.

### 결론에 포함할 것

근본 원인 + 해결 방법(또는 해결 불가 판정과 근거) 권고. 근거는 1차 자료 인용.

## Answer

전체 조사: `docs/research/office-tip-activation-gap.md` (브랜치 `research/office-tip-activation-gap`,
커밋 `0d59687`).

**근본 원인은 완전히 확정할 수 없음.** 가장 유력한 가설(중간 신뢰도, 추론 — Microsoft가 Excel의
TSF 통합 방식을 공개 문서화한 적이 없음): Excel 셀 에디터가 `ITextStoreACP`를 구현하지 않는
TSF-unaware 컨트롤이라 TSF의 "Transitory Context" 브리지 경로를 타고 있고, 이 경로는 "현재 선택된
TIP 하나"만 이어주면 충분해 선택되지 않은 관찰자 TIP은 `CoCreateInstance`조차 되지 않는다는 것.
Windows 표시기는 정확(선택된 MS IME는 활성화됨)하지만 우리 TIP은 `Activate()`조차 없다는 관찰된
두 사실과 모순 없이 들어맞는다.

- `GUID_TFCAT_TIPCAP_*` 공식 카테고리 목록(`Predefined Category Values`)에 Office 전용 게이트키퍼는
  존재하지 않음 — `GUID_TFCAT_TIP_KEYBOARD` 카테고리 추가만으로는 해결 안 된다는 기존 실측
  (`Registration.cpp`, 2026-08-07)이 문서와 모순되지 않음(애초에 해당 카테고리가 없으므로).
- `ActivateProfile`의 `TF_IPPMF_ENABLEPROFILE`(설치된 키보드 목록에 등록)과
  `FORPROCESS`/`FORSESSION`(지금 이 순간 실제 키 입력을 처리하는 "선택된" 프로필로 전환)은 문서상
  명확히 분리된 별개 개념.
- **완전 해결책("선택된 프로필 되기")은 CLAUDE.md 3절 비목표(IME 자체 동작 변경/가로채기 금지)와
  정면 충돌 — 배제.** "선택되면 다른 TIP은 밀려난다"는 TSF의 전제 자체가 실제 키 입력을 가로채는
  것을 의미하기 때문.
- **절충안**: `TF_IPPMF_ENABLEPROFILE`만 단독 시도(선택 전환 없이 "설치된 키보드 목록"에만 등록) —
  비목표를 건드리지 않고, 크래시/회귀 위험도 없는 저위험 실험. 다만 성공을 보장하는 1차 자료는 없음.

**권고**: `TF_IPPMF_ENABLEPROFILE` 저위험 실험을 먼저 시도. 실패하면 ADR-0005 Update가 이미 기록한
"알려진 제약"으로 수용 — CLAUDE.md의 방어 규칙(마지막 상태 유지, 크래시 없음)과 정합적이므로 안전하게
받아들일 수 있음.
