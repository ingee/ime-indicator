<#
.SYNOPSIS
    ime-indicator를 Release 구성으로 빌드하고 release/ 디렉토리를 새로 만든다.
.DESCRIPTION
    매번 release/를 통째로 지우고 새로 만든다 — 과거에 다른 방식(예: dotnet publish
    --self-contained)으로 만들어진 산출물이 섞여 남는 것을 원천 차단하기 위함이다.
    실행 중인 ImeIndicator.exe/ImeFocusHookLoader.exe가 있으면 먼저 종료한다 — 안 그러면
    FocusHook DLL이 파일 잠금으로 복사에 실패할 수 있다.
#>

$ErrorActionPreference = "Stop"
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

# release/를 통째로 지우고 새로 만든다 — 과거에 다른 방식으로 만들어진 산출물이 섞여
# 남는 것을 막기 위함이다. 단, ImeFocusHookDll.dll은 GET_MODULE_HANDLE_EX_FLAG_PIN으로
# 한 번 주입된 프로세스(특히 세션 내내 안 꺼지는 explorer.exe)에서 절대 언로드되지 않으므로,
# 그 프로세스가 살아있는 동안은 파일이 잠긴 채로 삭제/교체가 실패할 수 있다 — 이미 배포된
# 파일이 최신이었다면 내용 손실은 아니므로, 개별 항목 삭제 실패는 경고만 남기고 계속한다.
$releaseDir = Join-Path $repoRoot "release"
if (Test-Path $releaseDir) {
    Get-ChildItem $releaseDir -Recurse -Force | Sort-Object FullName -Descending | ForEach-Object {
        try {
            Remove-Item $_.FullName -Force -ErrorAction Stop
        } catch {
            Write-Warning "삭제 못함(다른 프로세스가 사용 중일 수 있음 — pin된 FocusHook DLL이면 정상): $($_.FullName)"
        }
    }
}
if (-not (Test-Path $releaseDir)) {
    New-Item -ItemType Directory -Path $releaseDir | Out-Null
}

# bin/Release/net8.0-windows/ 전체를 복사하지 않고 필요한 것만 골라 복사한다 — 예전에
# 그 폴더 안에서 dotnet publish -r win-x64 등 다른 명령을 실행한 잔재(win-x64/ 등)가
# bin/에 남아있어도, release/를 통째 복사하는 방식이었다면 그게 매번 다시 들어왔을 것이다
# (실제로 이 문제로 win-x64/가 release/에 계속 재생성되는 걸 발견했다). 화이트리스트 방식은
# bin/에 뭐가 더 쌓여도 release/는 항상 필요한 것만 갖는다.
$binDir = "$repoRoot\src\ImeIndicator\bin\Release\net8.0-windows"
$copyErrors = @()
foreach ($item in "ImeIndicator.exe", "ImeIndicator.dll", "ImeIndicator.pdb", "ImeIndicator.deps.json", "ImeIndicator.runtimeconfig.json", "FocusHook") {
    $source = Join-Path $binDir $item
    if (Test-Path $source) {
        Copy-Item $source $releaseDir -Recurse -Force -ErrorAction SilentlyContinue -ErrorVariable itemCopyErrors
        $copyErrors += $itemCopyErrors
    }
}
foreach ($copyError in $copyErrors) {
    Write-Warning "복사 못함(다른 프로세스가 사용 중일 수 있음 — pin된 FocusHook DLL이면 정상): $copyError"
}

Write-Output "release/ 갱신 완료: $releaseDir"
