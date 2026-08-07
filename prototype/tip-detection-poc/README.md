# tip-detection-poc — THROWAWAY 프로토타입

**질문**: TSF Text Input Processor(TIP)로 등록하면, 다른 프로세스의 실제 한/영 IME
상태(`GUID_COMPARTMENT_KEYBOARD_OPENCLOSE`)를 관찰할 수 있는가?

**답**: 그렇다 — 단, 문서/전역/컨텍스트가 아니라 **스레드 스코프**로 봐야 한다.

- **로딩은 된다** — 등록만 해두면(사용자가 키보드로 선택하지 않아도) 텍스트 입력을
  다루는 거의 모든 프로세스에 실제로 DLL이 로드되고 `Activate()`가 호출된다.
- **문서/전역/컨텍스트 스코프로는 관찰 안 됨** — `GUID_COMPARTMENT_KEYBOARD_OPENCLOSE`를
  이 세 스코프에서 다 구독해봐도, 컨텍스트에 존재하는 모든 컴파트먼트를 나열해 전부
  구독해봐도, 실제 한/영 전환 시 값이 전혀 바뀌지 않았고 `OnChange`도 오지 않았다
  (`TipPoc`~`TipPoc5`, 메모장·탐색기 양쪽 동일).
- **스레드 스코프로는 관찰됨** (`TipPoc6`) — `ITfThreadMgr::GetGlobalCompartment()`를
  거치지 않고 `ITfThreadMgr` 자신을 `ITfCompartmentMgr`로 직접 QI해서 얻는 컴파트먼트는
  실제 한/영 전환에 맞춰 0/1로 정확히 토글되고 `OnChange`도 정상적으로 온다. 다른
  프로세스는 영향받지 않아 앱마다 독립적이라는 기존 확인(ADR-0002)과도 일치한다.
- **다만 "어느 앱이 지금 포커스인가"를 판단하는 키는 PID여야 한다** (`TipPoc7`) — TIP이
  활성화된 스레드가 실제 창을 소유한 스레드와 항상 같지는 않다(클래식 Win32 앱은 같지만,
  Windows 11 패키지형 메모장 등 최신 앱은 다르다). 반면 PID는 모든 경우에 일치했다.

자세한 내용과 다음 방향은 [`docs/adr/0004-tip-prototype-inconclusive.md`](../../docs/adr/0004-tip-prototype-inconclusive.md)
참고 (`feature/ime-state-detection` 브랜치, ADR 갱신 예정).

## 버전 이력

`TipPoc.cpp` → `TipPoc5.cpp`까지 순차적으로 존재한다. 매번 등록한 DLL을 explorer.exe나
터미널 등 시스템 프로세스들이 이미 로드해가서 파일을 덮어쓸 수 없었기 때문에, 매번 새
CLSID/파일명으로 이어가며 실험했다 (자세한 경위는 원본 대화 기록 참고):

1. `TipPoc` — 1단계: DLL이 실제로 다른 프로세스에 로드되는지만 확인 (성공)
2. `TipPoc2` — 2단계: 문서 레벨 컴파트먼트 구독 추가 (변화 없음)
3. `TipPoc3` — 전역(thread-level) 컴파트먼트 비교 추가 (변화 없음)
4. `TipPoc4` — 컨텍스트(`ITfContext`) 레벨 컴파트먼트 비교 추가 (변화 없음)
5. `TipPoc5` — 컨텍스트의 모든 컴파트먼트를 `EnumCompartments`로 나열해 전부 구독 (변화 없음)
6. `TipPoc6` — 네 번째 스코프 추가: `ITfThreadMgr::GetGlobalCompartment()`를 거치지 않고
   `ITfThreadMgr` 자신을 `ITfCompartmentMgr`로 직접 QI해서 얻는 "스레드 스코프" 컴파트먼트.
   TipPoc3의 GLOBAL과는 별개의 저장소 — TIP이 로드되는 각 프로세스마다 별도의 `ITfThreadMgr`
   인스턴스를 받으므로, 이 스코프에 값이 있다면 앱마다 독립적으로 유지된다는(ADR-0002)
   기존 확인과도 부합할 것이라는 가설 검증. **→ 성공.** 메모장에서 실제 한/영 전환 시
   THREADSCOPE 값이 0/1로 정확히 토글되고 `OnChange`도 매번 정상적으로 옴. DOC/GLOBAL/CONTEXT는
   여전히 죽어있음. 다른 프로세스(pid)는 이 변화에 반응하지 않음 — 앱마다 독립적 상태라는
   기존 확인과 일치.
7. `TipPoc7` — TipPoc6가 성공한 뒤 남은 질문: UI 프로세스가 `GetForegroundWindow()` +
   `GetWindowThreadProcessId()`로 "지금 포커스 앱이 어느 프로세스인지" 판단할 때, 그 결과가
   TIP이 활성화된 스레드와 항상 일치하는지 검증(일치해야 "포커스 판단 결과"로 "어느 TIP
   인스턴스의 값을 보여줄지" 안전하게 매칭할 수 있음). `Activate()`/`OnSetFocus()`/`OnChange()`
   시점마다 자기 프로세스ID·스레드ID와 그 순간의 포그라운드 윈도우 프로세스ID·스레드ID를
   같이 로그로 남김. **결과: PID는 항상 일치하지만 TID는 앱마다 다르다.** 클래식 Win32
   단일 스레드 앱(mintty/Git Bash 터미널)은 TIP이 활성화된 스레드가 곧 창을 소유한
   스레드라 TID까지 일치했지만, Windows 11의 패키지형(WinUI) 메모장은 TIP이 활성화된
   스레드(예: tid=22856)와 실제 창을 소유한 스레드(tid=17248)가 서로 달라 TID는 항상
   불일치, PID만 일치. **결론: IPC에서 "어느 앱의 상태를 보여줄지" 매칭하는 키는 반드시
   PID를 써야 하고, TID를 키로 쓰면 최신 패키지형 앱에서 깨진다.**

`poc-log*.txt`는 각 버전 실행 시 실제로 남은 로그(실측 결과)다.

## 빌드/등록 (참고용 — 재실행하려면)

```powershell
.\build7.ps1                     # TipPoc7.dll 빌드
regsvr32 TipPoc7.dll             # 등록 (관리자 권한 필요할 수 있음)
regsvr32 /u TipPoc7.dll          # 등록 해제
```

등록 후 아무 텍스트 입력 프로세스나 새로 띄우면 `poc-log7.txt`에 로그가 남는다.
