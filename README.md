# ime-indicator

Windows용 한/영 입력 상태(IME) 인디케이터. 자세한 스펙은 [`CLAUDE.md`](CLAUDE.md), 구현
진행 상황은 [`CLAUDE_worklist.md`](CLAUDE_worklist.md) 참고.

## 빌드

저장소 루트에서:

```
dotnet build ImeIndicator.slnx
```

## 테스트

```
dotnet test ImeIndicator.slnx
```

## 실행

```
dotnet run --project src/ImeIndicator/ImeIndicator.csproj
```

또는 빌드 결과물을 직접 실행:

```
src/ImeIndicator/bin/Debug/net8.0-windows/ImeIndicator.exe
```

배포용 self-contained 단일 파일 게시:

```
dotnet publish src/ImeIndicator/ImeIndicator.csproj -r win-x64 --self-contained -p:PublishSingleFile=true
```

## 사용법 (현재까지 구현된 범위)

- 실행하면 모니터마다 인디케이터 창이 하나씩 상단 중앙에 뜬다(정사각형, 배경색으로
  한글/영문 표시).
- 인디케이터를 클릭하면 표시가 반전되고, 모든 모니터의 인디케이터가 함께 반전된다.
- 프로세스를 종료하려면 현재는 작업 관리자에서 `ImeIndicator.exe`를 강제 종료해야 한다
  (트레이 아이콘/종료 메뉴는 아직 구현 전).

### 알려진 문제

- **실제 시스템의 한/영 상태를 반영하지 못한다.** 현재 구현(TSF `ITfThreadMgrEventSink`
  구독)은 다른 프로세스의 포커스/컴파트먼트 변경 이벤트를 받지 못하는 것으로 확인됐다 —
  TSF의 스레드 매니저·컴파트먼트 객체가 그 인스턴스를 만든 프로세스 안에서만 유효하기
  때문으로 보인다. 즉 현재는 항상 시작 시 기본값(영문)으로만 뜨고, 클릭으로 수동 반전한
  경우를 빼면 실제 입력 상태와 무관하게 고정되어 있다.
- 원인과 검토한 대안은 대화 기록/커밋 히스토리 참고. 정식 해결(TSF Text Input Processor
  등록 등)은 다음 작업으로 남아 있다.
