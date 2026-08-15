<#
.SYNOPSIS
    ime-indicator를 Release 구성으로 빌드한다(세션 중 반복 실행용). release/ 디렉토리는
    건드리지 않는다.
.DESCRIPTION
    산출물은 src/ImeFocusHook/{x64,Win32}/Release/와
    src/ImeIndicator/bin/Release/net8.0-windows/(FocusHook/{x64,x86}/ 포함, ImeIndicator.csproj의
    CopyFocusHookBinaries 타겟이 자동 복사)에 그대로 남는다 — 세션 중 테스트는 이 bin 폴더의
    ImeIndicator.exe를 직접 실행해서 한다. release/는 세션을 마무리하며 이번 빌드를 "안정
    버전"으로 승격할 때만 scripts/release.ps1로 갱신한다.

    실행 중인 ImeIndicator.exe/ImeFocusHookLoader.exe가 있으면 먼저 종료한다 — 안 그러면
    FocusHook DLL이 파일 잠금으로 빌드(링크)에 실패할 수 있다. 단, 이미 다른 프로세스(예:
    mintty, explorer.exe)에 주입돼 GET_MODULE_HANDLE_EX_FLAG_PIN으로 pin된 DLL은 이 종료로도
    안 풀린다 — 그 프로세스가 살아있는 동안은 계속 잠긴 채로, 빌드 자체(링크 단계)가 실패할 수
    있다(CLAUDE.md "개발 중 자기 자신의 터미널을 잠그지 않으려면" 절 참고).
#>

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$repoRoot = Split-Path -Parent $PSScriptRoot

Get-Process -Name ImeIndicator, ImeFocusHookLoader -ErrorAction SilentlyContinue | Stop-Process -Force

$msbuild = Get-ChildItem "C:\Program Files\Microsoft Visual Studio", "C:\Program Files (x86)\Microsoft Visual Studio" `
    -Recurse -Filter MSBuild.exe -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
if (-not $msbuild) {
    throw "MSBuild.exe를 찾지 못했습니다. Visual Studio Build Tools(C++ 워크로드, MSVC v145 툴셋)가 설치돼 있는지 확인하세요."
}

# 네이티브 hook(x64 + Win32, Dll + Loader) Release 빌드 — UI 빌드보다 먼저 해야
# ImeIndicator.csproj의 내장 빌드 타겟이 그 결과물을 FocusHook/ 아래로 복사해간다.
foreach ($platform in "x64", "Win32") {
    foreach ($project in "ImeFocusHookDll", "ImeFocusHookLoader") {
        & $msbuild "$repoRoot\src\ImeFocusHook\$project.vcxproj" /p:Configuration=Release /p:Platform=$platform /nologo /v:minimal
        if ($LASTEXITCODE -ne 0) {
            throw "$project ($platform) 빌드 실패"
        }
    }
}

dotnet build "$repoRoot\src\ImeIndicator\ImeIndicator.csproj" -c Release --nologo -v:minimal
if ($LASTEXITCODE -ne 0) {
    throw "ImeIndicator.csproj 빌드 실패"
}

Write-Output "빌드 완료: src\ImeIndicator\bin\Release\net8.0-windows\ImeIndicator.exe (세션 중 테스트는 여기서 실행)"
