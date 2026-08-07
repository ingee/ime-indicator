#pragma once

// 이 DLL 안에서 살아있는 COM 객체 개수를 추적한다(DllCanUnloadNow 판단용).
void DllExports_AddDllRef();
void DllExports_ReleaseDllRef();
