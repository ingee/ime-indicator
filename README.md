# ime-indicator

Windows용 한/영 입력 상태(IME) 인디케이터.
화면 구석에 작은 창을 띄워 현재 입력 상태 (한글/영문)를 표시한다.


## 동작 방식

텍스트 입력 포커스가 바뀔 때마다 그 프로세스 안에 hook dll을 주입해 TSF 상태를 구독한다
([ADR-0007](docs/adr/0007-focus-triggered-incontext-injection.md)).
32비트 프로세스(Excel 등) 커버를 위해 64비트용과 32비트용 hook 로더가 함께 떠 있어야 한다.


## 실행

> 아래 `release/` 디렉토리에 이미 빌드 결과물이 있다고 가정한다.
> 처음부터 빌드하려면 아래 "빌드" 절을 참조한다.
> 실행하는 PC에 **.NET 8 Desktop Runtime**이 설치돼 있어야 한다

```
release\ImeIndicator.exe
```

실행하면 UI 프로세스(`ImeIndicator.exe`)가 hook 로더 두 개(`FocusHook\x64\
ImeFocusHookLoader.exe`, `FocusHook\x86\ImeFocusHookLoader.exe`)를 함께 띄운다 — 작업
관리자에 이 셋이 모두 보이면 정상이다.

> **알려진 제약(미검증)**: 로더를 관리자 권한 없이 실행하면, 관리자 권한으로 뜬 다른
> 프로세스(예: 관리자 권한 cmd, 일부 개발 도구)에는 Windows UIPI 때문에 hook이 안 들어가서
> 그 앱의 상태가 인디케이터에 반영되지 않을 수 있다.


## 사용법 (현재까지 구현된 범위)

- 실행하면 모니터마다 인디케이터 창이 하나씩 상단 중앙에 뜬다(정사각형, 한글/영문 상태를
  다른 배경색으로 표시).
- 인디케이터를 클릭하면 표시가 반전되고, 모든 모니터의 인디케이터가 함께 반전된다.
- 프로세스를 종료하려면 현재는 작업 관리자에서 `ImeIndicator.exe`를 강제 종료해야 한다
  (트레이 아이콘/종료 메뉴는 아직 구현 전). 종료하면 hook 로더 두 개도 함께 정리된다.


## 빌드

필요한 도구: .NET 8 SDK, Visual Studio Build Tools(C++ 워크로드, MSVC v145 툴셋).

1. 네이티브 hook(`src/ImeFocusHook/`)을 64비트(x64)와 32비트(Win32) 두 가지로 빌드한다
   (Developer PowerShell 등 MSVC 환경이 잡힌 셸에서):

   ```
   msbuild src/ImeFocusHook/ImeFocusHookDll.vcxproj    /p:Configuration=Release /p:Platform=x64
   msbuild src/ImeFocusHook/ImeFocusHookLoader.vcxproj /p:Configuration=Release /p:Platform=x64
   msbuild src/ImeFocusHook/ImeFocusHookDll.vcxproj    /p:Configuration=Release /p:Platform=Win32
   msbuild src/ImeFocusHook/ImeFocusHookLoader.vcxproj /p:Configuration=Release /p:Platform=Win32
   ```

2. UI(`src/ImeIndicator/`)를 빌드한다 — `ImeIndicator.csproj`에 내장된 빌드 타겟이 위
   네이티브 산출물을 `bin/Release/net8.0-windows/FocusHook/{x64,x86}/`로 자동 복사한다
   (아직 없어도 경고만 내고 UI는 정상 빌드됨):

   ```
   dotnet build src/ImeIndicator/ImeIndicator.csproj -c Release
   ```


## release 디렉토리 만들기

저장소 루트의 `release/`는 실행에 필요한 산출물을 모아두는 곳이다. 소스 관리 대상이
아니며(`.gitignore`), 위 빌드 후 매번 새로 만든다:

```powershell
Copy-Item -Recurse -Force src/ImeIndicator/bin/Release/net8.0-windows/* release/
```


## 테스트

```
dotnet test ImeIndicator.slnx
```


## 개발/검증 환경

- Windows 11 Pro. 입력기는 TSF(Text Services Framework)로만 동작하며, 레거시 IMM API
  (`ImmGetContext`)는 `himc=0`을 반환해 사실상 쓸 수 없다.
- 한국어 키보드의 **"이전 버전의 Microsoft IME 사용" 옵션: 켜짐**(설정 > 시간 및 언어 > 언어
  및 지역 > 한국어 > 키보드 옵션). 이 옵션 상태에 따라 IME 이벤트 동작이 달라질 수 있으므로,
  다른 환경에서 개발/검증할 때는 이 값도 함께 맞추는 것을 권장한다.
