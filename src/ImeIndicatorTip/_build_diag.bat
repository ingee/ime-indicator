@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cd /d "C:\_data\git\ime-indicator\src\ImeIndicatorTip"
cl.exe /EHsc /nologo _diag_check_enabled.cpp /Fe:_diag_check_enabled.exe ole32.lib
