# Office(Excel)에서 관찰자 TIP의 Activate()가 호출되지 않는 이유 — 1차 자료 조사

Type: research
Status: done
Source issue: `.scratch/ime-focus-accuracy/issues/02-office-tip-activation-gap.md`

## 조사 범위와 한계 (먼저 밝힘)

이 문서가 인용하는 1차 자료는 다음 세 그룹이다.

1. **Win32 공식 레퍼런스** (`learn.microsoft.com/.../win32/tsf/...`, `.../win32/api/msctf/...`) — 현재도
   유지되는 공식 API 문서.
2. **TSF 팀 자체 블로그(`blogs.msdn.com/tsfaware`, `blogs.msdn.com/alexdan`)의 Microsoft Learn 아카이브
   미러** — TSF를 만든 팀이 2007~2009년에 직접 쓴 글로, 지금은 "archived" 표시가 붙어 있지만 API
   레퍼런스가 다루지 않는 "왜 이렇게 동작하는가"를 설명하는 사실상 유일한 1차 자료다. 저자는 TSF
   개발팀 소속(`kexugit`, MSDN 블로그 표준 게시 계정)이며, 다른 2차 자료(Mozilla Bugzilla, 각종 IME
   구현체 문서)들이 이 블로그를 반복 인용하는 것으로 교차 확인된다.
3. **이 저장소의 실측 기록** (`docs/adr/0005-...md`, `src/ImeIndicatorTip/*.cpp`) — 코드와 ADR에 남은
   1차 실측 데이터.

**한계**: Microsoft는 Office(Excel)의 셀 그리드 편집기가 TSF를 어떤 방식으로 사용하는지 — 특히
`ITextStoreACP`를 구현한 진짜 TSF-aware 컨트롤인지, 아니면 TSF가 레거시 컨트롤에 제공하는
"Transitory Context" 브리지를 타는지 — 를 공개 문서화한 적이 없다(검색으로 확인). Office는
클로즈드 소스이고, 이 질문에 답하는 Microsoft 공식 문서나 신뢰할 만한 리버스 엔지니어링 자료를
찾지 못했다. 아래 4번 질문에 대한 답은 **문서로 확인된 TSF의 일반 동작 원리로부터의 추론**이며,
Excel 내부 구현을 직접 확인한 것이 아니라는 점을 명시한다.

---

## 질문 1 — TSF가 "이 프로세스/스레드에 어떤 TIP을 로드할지" 결정하는 조건

### 확인된 사실

- 키보드 TIP으로 동작하려면 `ITfCategoryMgr::RegisterCategory(clsid, GUID_TFCAT_TIP_KEYBOARD, clsid)`로
  카테고리 등록을 해야 한다는 것은 Microsoft 자체 샘플(SampleIME)과 TSF 팀 블로그가 공통으로
  전제하는 최소 조건이다. ["What's a Keyboard?"](https://learn.microsoft.com/en-us/archive/blogs/tsfaware/whats-a-keyboard)
  (TSF 팀 블로그, 2007) 첫 문장: *"The Text Services Framework makes a number of assumptions when you
  register your text service as a keyboard text service (i.e., your text service calls
  RegisterCategory(&lt;clsid of your text service&gt;, GUID_TFCAT_TIP_KEYBOARD, &lt;clsid of your text
  service&gt;)."*
- 같은 글의 핵심 주장: **"TSF assumes that exactly one keyboard text service can be active at a time.
  If the user activates one keyboard text service, all other keyboard text services will be
  disabled."** — 다만 여기서 "disabled"는 인스턴스가 아예 생성되지 않는다는 뜻이 아니라, 바로 다음
  문단이 설명하는 두 컴파트먼트로 세분화된다:
  - `GUID_COMPARTMENT_KEYBOARD_DISABLED` — **컨텍스트(context) 단위** 스코프. 0이 아니면 "이
    TIP은 키 입력을 해석하지 말고 그대로 통과시켜라"는 뜻.
  - `GUID_COMPARTMENT_KEYBOARD_OPENCLOSE` — **스레드(thread manager) 단위** 스코프. 이 저장소의
    ADR-0005가 실측으로 확인한 것과 정확히 일치하며, [Predefined
    Compartments](https://learn.microsoft.com/en-us/windows/win32/tsf/predefined-compartments) 공식
    문서도 "This compartment is specific to a thread manager object"라고 명시해 교차 확인된다.
  - 따라서 "exactly one active"는 **키 입력을 실제로 가로채 변환하는 TIP이 한 번에 하나뿐**이라는
    뜻이지, 카테고리에 등록된 다른 키보드 TIP들이 그 스레드에 아예 `CoCreateInstance`/`Activate`
    되지 않는다는 뜻이 아니다. 이 저장소의 실측(ADR-0005: 메모장·Notepad++·mintty·패키지형
    메모장에서 관찰자 TIP도 정상적으로 `Activate`됨)과도 부합한다.
- `ITfTextInputProcessor::Activate`
  [공식 문서](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itftextinputprocessor-activate)
  Remarks: *"TSF calls this method after creating an instance of a text service with a call to
  CoCreateInstance."* — 즉 "이 스레드에 이 TIP을 인스턴스화할지 말지" 자체가 TSF 내부(msctf.dll)의
  판단이며, 그 판단 알고리즘의 세부 규칙은 이 문서에도, 다른 어떤 공식 문서에도 실려 있지 않다.
  **이 부분은 Microsoft가 공개 문서화하지 않은 내부 동작**이라는 것이 이번 조사의 명확한 결론이다.
- [Predefined Category Values](https://learn.microsoft.com/en-us/windows/win32/tsf/predefined-category-values)
  공식 문서에 정리된 `GUID_TFCAT_TIPCAP_*` 목록은 다음과 같다(전문 인용):
  - `GUID_TFCAT_TIPCAP_SECUREMODE` — 보안 데스크톱에서 실행 가능
  - `GUID_TFCAT_TIPCAP_UIELEMENTENABLED` — `ITfUIElement`를 구현해 UI를 그림
  - `GUID_TFCAT_TIPCAP_INPUTMODECOMPARTMENT` — input mode compartment 지원
  - `GUID_TFCAT_TIPCAP_COMLESS` — COM 없이 활성화 가능
  - `GUID_TFCAT_TIPCAP_WOW16` — 16비트 태스크에서 활성화 가능
  - `GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT` — Windows Store(패키지형) 앱에서 정상 동작
  - `GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT` — 시스템 트레이 포함 지원
  - 이 중 **Office/Excel 전용, 혹은 "그리드/셀 편집기에서 로드되려면 이 카테고리가 필요하다"는
    카테고리는 존재하지 않는다.** 목록 전체가 "보안 데스크톱", "UI 표시 방식", "패키지형 앱 지원"
    등 다른 축의 능력(capability)이며 Office 특이적인 게이트키퍼는 문서상 없다.

### 결론 (질문 1)

문서로 확인 가능한 조건은 "`GUID_TFCAT_TIP_KEYBOARD` 카테고리 등록 + 스레드의 현재 입력 언어와
프로필의 langid가 일치"뿐이다(질문 2에서 다룸). Office가 요구하는 추가 `GUID_TFCAT_TIPCAP_*`는
공식 카테고리 목록에 없으므로, 이 프로젝트가 이미 실측한 "카테고리 추가만으로는 해결 안 됨"
(`Registration.cpp` 주석, 2026-08-07)이라는 결과와 문서상 모순되지 않는다 — **애초에 해당하는
카테고리가 존재하지 않는다.**

---

## 질문 2 — `ITfInputProcessorProfileMgr`/`ActivateProfile`로 "선택 가능한 프로필"로 등록해야만
Office에서 로드되는가

### 확인된 사실

- [Text Service Registration](https://learn.microsoft.com/en-us/windows/win32/tsf/text-service-registration)
  공식 문서(등록 절차의 정본)의 "Registering Language Profiles" 절, 핵심 문장: **"A text service is
  only available when an application has the focus and the proper language is selected in the
  language bar."** — 이 조건은 "특정 TIP이 선택돼야 한다"가 아니라 **"그 TIP이 등록된 언어(langid)가
  현재 스레드의 입력 언어로 선택돼 있어야 한다"**는 뜻으로 읽힌다. 이 프로젝트의 TIP은 이미
  `kKoreanLangId = 0x0412`로 `AddLanguageProfile`을 호출하고 있고(`Registration.cpp:60-67`), 실측상
  대부분의 앱에서 정상적으로 `Activate`되므로 이 조건 자체는 이미 충족되고 있다고 볼 수 있다.
- 코드가 사용하는 API는 **TSF 1.0 시절의 구식 인터페이스**인
  [`ITfInputProcessorProfiles::AddLanguageProfile`](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfinputprocessorprofiles-addlanguageprofile)이다.
  이 메서드에는 "enable/disable" 플래그 파라미터 자체가 없다(시그니처 확인: `rclsid, langid,
  guidProfile, pchDesc, cchDesc, pchIconFile, cchFile, uIconIndex` — `dwFlags` 없음). 즉 이 API
  수준에서는 애초에 "등록"과 "활성화 가능 상태"가 분리되어 있지 않다 — 등록하면 곧바로 이용 가능
  후보가 된다는 것이 API 설계다.
- Vista 이후 추가된 신형 인터페이스
  [`ITfInputProcessorProfileMgr::ActivateProfile`](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfinputprocessorprofilemgr-activateprofile)은
  `dwFlags`로 다음을 구분한다(공식 문서 표 전문):
  - `TF_IPPMF_ENABLEPROFILE` — *"Update the registry to enable this profile for this user."*
    (사용자별 "설치된 키보드 목록"에 추가하는 것 — Windows 설정의 "키보드 추가"와 동급)
  - `TF_IPPMF_FORPROCESS` / `TF_IPPMF_FORSESSION` — 프로필을 **그 순간의 "선택된(현재 입력을
    처리하는)" 프로필로 전환**하는 스코프 지정(프로세스 전체 vs 데스크톱 전체)
  - `TF_IPPMF_DONTCARECURRENTINPUTLANGUAGE` — 현재 입력 언어와 다르면 나중에 그 언어로 전환될 때
    지연 활성화
  - 반환값 `S_FALSE`의 의미: *"The language profile is not enabled."* — 즉 "enable"과 "activate(현재
    선택으로 전환)"는 이 API에서도 **별개의 두 단계**로 명확히 구분된다.
  - **`ActivateProfile`(특히 `TF_IPPMF_FORPROCESS`/`FORSESSION`)로 "현재 선택된 프로필"로 전환하는
    것은 그 순간부터 실제 키 입력을 그 TIP이 처리하게 만드는 행위다.** 이는 "관찰만 하고 IME 동작을
    가로채지 않는다"는 이 프로젝트의 비목표와 정면으로 충돌한다(질문 4에서 다시 다룸).
  - 반면 `TF_IPPMF_ENABLEPROFILE`만 단독으로 사용하는 것은 "설치된 키보드 목록에 추가"일 뿐,
    "지금 이 순간 어떤 프로필이 키 입력을 처리하는가"는 건드리지 않는다 — 관찰자 역할을 유지하면서
    시도해볼 수 있는 여지가 있다(단, 이 저장소 코드는 구식 `ITfInputProcessorProfiles` 계열만
    쓰고 있어 `ITfInputProcessorProfileMgr`/`RegisterProfile`/`EnableProfile` 계열은 아직 호출한 적이
    없다 — 미검증).

### 결론 (질문 2)

문서상 "선택된 프로필로의 전환(`ActivateProfile` + `FORPROCESS`/`FORSESSION`)"과 "언어별 설치 목록에
enable로 등록(`TF_IPPMF_ENABLEPROFILE`)"은 별개다. **"실제 키 입력을 처리하는 선택 IME가 되는 것"은
확실히 비목표와 충돌하므로 배제해야 한다.** 반면 **"enable로만 등록"은 아직 이 프로젝트가 시도하지
않은, 비목표와 충돌하지 않는 미검증 후보**다 — 다만 이것이 Office의 필터링을 통과시켜 줄지는 어떤
1차 자료도 보장하지 않는다(질문 3의 한계 참고).

---

## 질문 3 — Office(엑셀)의 그리드/셀 인플레이스 에디터가 TIP 목록을 특별히 필터링하는 방식이
문서화돼 있는가

### 확인된 사실 — "그렇다/아니다"를 직접 말하는 1차 자료는 찾지 못했다

Microsoft Learn, MSDN 아카이브, Win32 공식 레퍼런스 어디에도 Excel(또는 다른 Office 그리드 계열
컨트롤)의 TSF 통합 방식을 다루는 문서는 없다. 이는 검색 실패가 아니라 **Office 내부 구현이
비공개이고, TSF 팀 블로그도 자기 팀 API만 다루지 특정 ISV(Office 포함)의 채택 방식은 다루지
않기 때문**으로 보인다.

### 정황상 가장 근접한 1차 자료 — Transitory Context / Transitory Extension

TSF 팀 블로그가 문서화한, "TSF-aware가 아닌 컨트롤"이 TSF와 상호작용하는 일반 메커니즘은 다음과
같다. 이것이 Excel 사례와 정확히 일치한다고 확인된 것은 아니지만, 관찰된 증상과 정합적인 유일한
문서화된 메커니즘이다.

- [Transitory Contexts](https://learn.microsoft.com/en-us/archive/blogs/tsfaware/transitory-contexts)
  (TSF 팀 블로그, 2007): *"Transitory contexts are contexts managed by the TSF manager for
  applications that aren't TSF-Aware."* 그리고 *"there's exactly **one** transitory context; it gets
  re-attached to different windows as focus moves between controls... when the transitory context
  gets re-attached to a different window, the thread manager is not involved."*
- [Transitory Extensions, or, how to get full text store support in TSF-unaware
  controls](https://learn.microsoft.com/en-us/archive/blogs/tsfaware/transitory-extensions-or-how-to-get-full-text-store-support-in-tsf-unaware-controls)
  (TSF 팀 블로그, 2007): *"TSF provides very basic services in applications that are not TSF-aware.
  In particular, TSF provides only transitory documents and contexts that represent short-lived
  text compositions. In Windows Vista, TSF adds full text store support for some frequently-used
  text controls - plain edit controls, richedit controls (that don't enable TSF), and Trident edit
  controls in Internet Explorer."* — 즉 Microsoft가 명시적으로 "완전한 text store 지원"을 추가해준
  대상은 **일반 Win32 EDIT 컨트롤, RichEdit, IE의 Trident 편집 컨트롤 세 가지뿐**이며, Excel의 셀
  그리드 편집기는 이 열거에 들어있지 않다(포함된다는 근거도, 제외된다는 명시적 근거도 없음 — 단순히
  언급이 없다).
  - 또한 이 메커니즘은 "TSF-aware 여부를 판별하려면 `ITfContext::GetStatus()`로
    `TF_SS_TRANSITORY` 비트를 확인하라"고 알려준다 — 즉 TSF-aware 여부는 공식적으로 컨트롤이
    `ITextStoreACP`를 구현해 진짜 `ITfContext`/`ITfDocumentMgr`를 제공하느냐로 갈린다(교차
    출처: Mozilla TSFTextStore 구현 논의에서도 "An application (or text control) must implement
    ITextStoreACP to be considered 'TSF-aware'"라는 동일한 이해가 반복됨 — 이건 2차 자료이므로
    참고용으로만 인용).

### 추론 (1차 자료로 확정되지 않음 — 신뢰도 낮음/중간으로 표시)

Office 2010의 셀 인플레이스 에디터가 RichEdit/표준 EDIT 계열이 아닌 자체 그리기 컨트롤이라는 점은
업계에 널리 알려진 사실이지만, 이를 명시하는 Microsoft 1차 자료는 찾지 못했다. **가설**: Excel의
셀 에디터가 (a) `ITextStoreACP`를 구현하지 않는 TSF-unaware 컨트롤이라 (b) TSF가 만들어주는
"Transitory Context" 경로로 키 입력을 처리하고 있고, 이 경로는 "지금 선택된 하나의 키보드 TIP"만
이어주면 충분하므로 TSF가 카테고리에 등록된 **모든** 키보드 TIP을 그 스레드에 인스턴스화할 필요를
못 느껴, 우리처럼 "선택되지 않은 관찰자" TIP은 애초에 `CoCreateInstance`조차 되지 않는다 — 는
설명이 사용자가 관찰한 두 사실(① Windows 표시기는 정확 — 선택된 MS IME는 활성화됨, ② 우리 TIP은
`Activate()`조차 없음)과 모순 없이 들어맞는다. 다만 이는 **문서로 확정된 사실이 아니라, 문서화된
일반 메커니즘으로부터의 추론**이며, Excel이 실제로 이 경로를 타는지 Microsoft가 확인해준 적은
없다.

---

## 질문 4 — "선택 가능한 활성 IME로 등록"이 유일한 해법이라면 비목표와 충돌하는가; 절충 가능한가

### 결론

- **완전히 충돌한다 — 만약 진짜로 "선택된 프로필(현재 키 입력을 처리하는 프로필)"이 되어야만
  Office가 로드해준다면, 그 순간부터 이 TIP은 실제 키 입력을 받아 한/영 변환을 수행하는 책임을
  져야 한다.** `ActivateProfile`의 `TF_IPPMF_FORPROCESS`/`FORSESSION`이 하는 일이 정확히 이것이고,
  "exactly one keyboard TIP can be active"([What's a
  Keyboard?](https://learn.microsoft.com/en-us/archive/blogs/tsfaware/whats-a-keyboard))라는 TSF의
  전제 자체가 "선택되면 다른 하나는 밀려난다"는 상호배타 관계를 의미한다. 이는 CLAUDE.md 3절
  비목표("IME 자체의 동작을 변경하거나 가로채는 기능 — 표시 전용")를 직접 위반한다: 사용자가 실제
  쓰는 Microsoft IME를 밀어내거나, 우리 TIP이 스스로 완전한 한/영 변환 엔진을 구현해야 하는(비현실적)
  상황이 된다.
- **절충 가능성 — `TF_IPPMF_ENABLEPROFILE`만 시도**: 질문 2에서 확인했듯 "enable(설치된 키보드
  목록에 등록)"과 "activate(지금 선택되어 키 입력 처리)"는 문서상 별개 개념이다. `ActivateProfile`을
  `TF_IPPMF_ENABLEPROFILE` 플래그만으로 호출해 "이 컴퓨터/사용자에 설치된 한국어 키보드 중 하나"로
  등록되게 하되, `FORPROCESS`/`FORSESSION`(선택 전환)은 호출하지 않는 절충은 이론적으로 비목표를
  건드리지 않는다 — 그러나 이 조합이 실제로 Office의 TIP 목록에 포함시켜 주는지는 어떤 1차 자료도
  보장하지 않는다(질문 3의 한계 참고). **시도해볼 가치는 있지만 성공을 보장할 수 없는 실험적
  단계**로만 취급해야 한다.

### 권고

1. **근본 원인**: 확정할 수 없다. 가장 유력한 설명(중간 신뢰도, 추론)은 "Excel 셀 에디터가
   `ITextStoreACP`를 구현하지 않는 TSF-unaware 컨트롤이라 TSF의 Transitory Context 경로를 타고
   있고, 이 경로는 '현재 선택된 TIP 하나'만 필요로 해서 관찰자 TIP은 인스턴스화되지 않는다"는
   것이다. 이는 Microsoft가 명시적으로 확인해준 적 없는 추론이다.
2. **당장 시도해볼 만한 저위험 실험** (비목표 위반 없음): `ITfInputProcessorProfileMgr::RegisterProfile`
   + `ActivateProfile(..., TF_IPPMF_ENABLEPROFILE)`로 프로필을 "설치된 키보드"로 승격시키는 것 —
   현재 코드는 구식 `ITfInputProcessorProfiles::AddLanguageProfile`만 쓰고 이 단계를 거친 적이
   없다. 성공 여부는 실측으로만 확인 가능하며, 실패하더라도 크래시나 회귀 위험은 없다(등록 단계의
   추가 호출일 뿐).
3. **그래도 안 될 경우**: ADR-0005 Update가 이미 기록한 대로 "알려진 제약"으로 문서화하고 받아들이는
   것이 CLAUDE.md의 방어 규칙(마지막으로 알려진 상태 유지, 크래시 없음)과 정합적이다. "선택된 프로필
   되기"로 완전히 해결하는 방법은 존재하지만, 그 대가(비목표 위반 — 실제 IME 가로채기)가 이
   프로젝트의 근본 설계 원칙과 맞지 않으므로 권장하지 않는다.

---

## 출처 목록

- [Predefined Category Values](https://learn.microsoft.com/en-us/windows/win32/tsf/predefined-category-values) — Win32 공식 레퍼런스
- [Text Service Registration](https://learn.microsoft.com/en-us/windows/win32/tsf/text-service-registration) — Win32 공식 레퍼런스
- [ITfInputProcessorProfiles::AddLanguageProfile](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfinputprocessorprofiles-addlanguageprofile) — Win32 공식 레퍼런스
- [ITfInputProcessorProfileMgr::ActivateProfile](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itfinputprocessorprofilemgr-activateprofile) — Win32 공식 레퍼런스
- [ITfTextInputProcessor::Activate](https://learn.microsoft.com/en-us/windows/win32/api/msctf/nf-msctf-itftextinputprocessor-activate) — Win32 공식 레퍼런스
- [Predefined Compartments](https://learn.microsoft.com/en-us/windows/win32/tsf/predefined-compartments) — Win32 공식 레퍼런스
- [What's a Keyboard?](https://learn.microsoft.com/en-us/archive/blogs/tsfaware/whats-a-keyboard) — TSF 팀 블로그(2007), Microsoft Learn 아카이브 미러
- [Transitory Contexts](https://learn.microsoft.com/en-us/archive/blogs/tsfaware/transitory-contexts) — TSF 팀 블로그(2007), Microsoft Learn 아카이브 미러
- [Transitory Extensions, or, how to get full text store support in TSF-unaware controls](https://learn.microsoft.com/en-us/archive/blogs/tsfaware/transitory-extensions-or-how-to-get-full-text-store-support-in-tsf-unaware-controls) — TSF 팀 블로그(2007), Microsoft Learn 아카이브 미러
- [Investigation: Text Service Framework (TSF) and Keyboards](https://learn.microsoft.com/en-us/archive/blogs/alexdan/investigation-text-service-framework-tsf-and-keyboards) — Microsoft 직원 블로그(2009), Microsoft Learn 아카이브 미러 (보조 확인용)
- `docs/adr/0005-thread-scope-compartment-with-pid-matching.md` — 이 저장소의 실측 기록
- `src/ImeIndicatorTip/Registration.cpp`, `src/ImeIndicatorTip/ImeStateTip.cpp` — 이 저장소의 현재 구현
