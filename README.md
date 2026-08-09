# ime-indicator

Windows용 한/영 입력 상태(IME) 인디케이터. 화면 구석에 작은 창을 항상 띄워 현재 입력 상태
(한글/영문)를 표시한다.

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

## 개발/검증 환경

- Windows 11 Pro. 입력기는 TSF(Text Services Framework)로만 동작하며, 레거시 IMM API
  (`ImmGetContext`)는 `himc=0`을 반환해 사실상 쓸 수 없다.
- 한국어 키보드의 **"이전 버전의 Microsoft IME 사용" 옵션: 켜짐**(설정 > 시간 및 언어 > 언어
  및 지역 > 한국어 > 키보드 옵션). 이 옵션 상태에 따라 IME 이벤트 동작이 달라질 수 있으므로,
  다른 환경에서 개발/검증할 때는 이 값도 함께 맞추는 것을 권장한다.
