# build5.ps1 — TipPoc5 프로토타입 빌드 스크립트 (THROWAWAY)
$ErrorActionPreference = "Stop"
$here = $PSScriptRoot
$vcvars = "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"

$cmd = "`"$vcvars`" x64 && cl.exe /LD /EHsc /std:c++17 /W3 /Fe:`"$here\TipPoc5.dll`" `"$here\TipPoc5.cpp`" /link /DEF:`"$here\TipPoc5.def`" ole32.lib advapi32.lib oleaut32.lib"
cmd /c $cmd
