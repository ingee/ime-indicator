# tip-detection-poc — THROWAWAY 프로토타입

**질문**: TSF Text Input Processor(TIP)로 등록하면, 다른 프로세스의 실제 한/영 IME
상태(`GUID_COMPARTMENT_KEYBOARD_OPENCLOSE`)를 관찰할 수 있는가?

**답**: 절반만 그렇다.

- **로딩은 된다** — 등록만 해두면(사용자가 키보드로 선택하지 않아도) 텍스트 입력을
  다루는 거의 모든 프로세스에 실제로 DLL이 로드되고 `Activate()`가 호출된다.
- **상태 관찰은 안 된다** — `GUID_COMPARTMENT_KEYBOARD_OPENCLOSE`를 문서/전역/컨텍스트
  세 스코프에서 다 구독해봐도, 그리고 컨텍스트에 존재하는 모든 컴파트먼트를 나열해
  전부 구독해봐도, 실제 한/영 전환 시 값이 전혀 바뀌지 않았고 `OnChange`도 오지 않았다.
  메모장(최신)과 탐색기 실행 창(클래식 Win32) 양쪽에서 동일했다.

자세한 내용과 다음 방향은 [`docs/adr/0004-tip-prototype-inconclusive.md`](../../docs/adr/0004-tip-prototype-inconclusive.md)
참고 (`feature/ime-state-detection` 브랜치).

## 버전 이력

`TipPoc.cpp` → `TipPoc5.cpp`까지 순차적으로 존재한다. 매번 등록한 DLL을 explorer.exe나
터미널 등 시스템 프로세스들이 이미 로드해가서 파일을 덮어쓸 수 없었기 때문에, 매번 새
CLSID/파일명으로 이어가며 실험했다 (자세한 경위는 원본 대화 기록 참고):

1. `TipPoc` — 1단계: DLL이 실제로 다른 프로세스에 로드되는지만 확인 (성공)
2. `TipPoc2` — 2단계: 문서 레벨 컴파트먼트 구독 추가 (변화 없음)
3. `TipPoc3` — 전역(thread-level) 컴파트먼트 비교 추가 (변화 없음)
4. `TipPoc4` — 컨텍스트(`ITfContext`) 레벨 컴파트먼트 비교 추가 (변화 없음)
5. `TipPoc5` — 컨텍스트의 모든 컴파트먼트를 `EnumCompartments`로 나열해 전부 구독 (변화 없음)

`poc-log*.txt`는 각 버전 실행 시 실제로 남은 로그(실측 결과)다.

## 빌드/등록 (참고용 — 재실행하려면)

```powershell
.\build5.ps1                     # TipPoc5.dll 빌드
regsvr32 TipPoc5.dll             # 등록 (관리자 권한 필요)
regsvr32 /u TipPoc5.dll          # 등록 해제
```

등록 후 아무 텍스트 입력 프로세스나 새로 띄우면 `poc-log5.txt`에 로그가 남는다.
