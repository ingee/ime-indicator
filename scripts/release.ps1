<#
.SYNOPSIS
    ime-indicator를 다시 빌드하고 release/ 디렉토리를 새로 만든다 — 세션을 마무리하며 지금까지의
    변경을 "안정 버전"으로 승격할 때만 실행한다.
.DESCRIPTION
    scripts/build.ps1로 먼저 다시 빌드해 release/가 항상 최신 소스를 반영하게 한 뒤, 매번
    release/를 통째로 지우고 새로 만든다 — 과거에 다른 방식(예: dotnet publish
    --self-contained)으로 만들어진 산출물이 섞여 남는 것을 원천 차단하기 위함이다.

    세션 중 반복 빌드/테스트는 scripts/build.ps1과 bin 폴더에서 하고, 이 스크립트는 그 빌드가
    release/의 현재 버전보다 완성도가 높다고 판단될 때(대개 wrap-up 시점) 한 번만 실행한다.
#>

$ErrorActionPreference = "Stop"
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$repoRoot = Split-Path -Parent $PSScriptRoot

& "$PSScriptRoot\build.ps1"

# release/를 통째로 지우고 새로 만든다 — 과거에 다른 방식으로 만들어진 산출물이 섞여
# 남는 것을 막기 위함이다. 단, ImeFocusHookDll.dll은 GET_MODULE_HANDLE_EX_FLAG_PIN으로
# 한 번 주입된 프로세스(특히 세션 내내 안 꺼지는 explorer.exe)에서 절대 언로드되지 않으므로,
# 그 프로세스가 살아있는 동안은 파일이 잠긴 채로 삭제/교체가 실패할 수 있다 — 이미 배포된
# 파일이 최신이었다면 내용 손실은 아니므로, 개별 항목 삭제 실패는 경고만 남기고 계속한다.
$releaseDir = Join-Path $repoRoot "release"
if (Test-Path $releaseDir) {
    Get-ChildItem $releaseDir -Recurse -Force | Sort-Object FullName -Descending | ForEach-Object {
        try {
            Remove-Item $_.FullName -Force -Confirm:$false -ErrorAction Stop
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
