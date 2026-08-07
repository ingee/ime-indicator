# build.ps1 — TipPoc 프로토타입 빌드 스크립트 (THROWAWAY)
$ErrorActionPreference = "Stop"
$here = $PSScriptRoot
$vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"

if (-not (Test-Path $vcvars)) {
    throw "vcvarsall.bat not found at $vcvars — MSVC Build Tools 경로를 확인하세요."
}

$cmd = "`"$vcvars`" x64 && cl.exe /LD /EHsc /std:c++17 /W3 /Fe:`"$here\TipPoc.dll`" `"$here\TipPoc.cpp`" /link /DEF:`"$here\TipPoc.def`" ole32.lib advapi32.lib"
cmd /c $cmd
