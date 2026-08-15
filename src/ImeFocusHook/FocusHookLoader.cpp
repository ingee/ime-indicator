// FocusHookLoader.cpp
//
// ADR-0007: 시스템 전역(idProcess=0)으로 EVENT_SYSTEM_FOREGROUND를 WINEVENT_INCONTEXT로
// 걸어, 포커스가 바뀔 때마다 그 순간 포커스를 얻은 프로세스 안에 ImeFocusHookDll.dll을 직접
// 로드시킨다. WINEVENT_INCONTEXT는 이 프로세스와 같은 비트니스의 대상에만 인프로세스로
// 주입되므로(실측 확인), 32비트 프로세스(Office 등)까지 커버하려면 이 EXE를 x64/x86 두
// 비트니스로 각각 빌드해 동시에 띄워야 한다 — src/ImeIndicator/Program.cs가 둘 다 기동한다.
//
// prototype/langbar-observation-poc-throwaway 브랜치의 LangBarPoc10/LangBarPoc10-x86와
// 동일한 메커니즘의 프로덕션 버전. UI가 없는 상주 프로세스라 콘솔을 띄우지 않는다
// (/SUBSYSTEM:WINDOWS, WinMain 진입점).

#include <windows.h>

#include <cwchar>

typedef void(CALLBACK* WINEVENTPROC_T)(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD);

#if defined(_WIN64)
static const wchar_t* kMutexName = L"ingee.ImeIndicator.FocusHookLoader.x64";
#else
static const wchar_t* kMutexName = L"ingee.ImeIndicator.FocusHookLoader.x86";
#endif

static const wchar_t* kDllFileName = L"ImeFocusHookDll.dll";

// 자기 자신의 EXE와 같은 폴더에서 DLL을 찾는다 — 배포 위치가 바뀌어도 절대 경로를 다시
// 손보지 않아도 되게 하기 위함.
static bool BuildDllPath(wchar_t* outPath, DWORD outPathSize) {
    wchar_t exePath[MAX_PATH] = {};
    DWORD len = GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    if (len == 0 || len == MAX_PATH) return false;

    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (!lastSlash) return false;
    *(lastSlash + 1) = L'\0';

    return swprintf_s(outPath, outPathSize, L"%s%s", exePath, kDllFileName) >= 0;
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    // 이미 이 비트니스의 로더가 실행 중이면 즉시 종료 — 재시작/중복 실행 시 훅이 중복
    // 등록되는 것을 막는다.
    HANDLE mutex = CreateMutexW(nullptr, TRUE, kMutexName);
    if (!mutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        return 1;
    }

    wchar_t dllPath[MAX_PATH] = {};
    if (!BuildDllPath(dllPath, MAX_PATH)) {
        return 1;
    }

    HMODULE hMod = LoadLibraryW(dllPath);
    if (!hMod) {
        return 1;
    }
    WINEVENTPROC_T pfn = reinterpret_cast<WINEVENTPROC_T>(GetProcAddress(hMod, "FocusHookProc"));
    if (!pfn) {
        return 1;
    }

    HWINEVENTHOOK hHook = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, hMod,
                                           pfn, 0, 0, WINEVENT_INCONTEXT);
    if (!hHook) {
        return 1;
    }

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    UnhookWinEvent(hHook);
    ReleaseMutex(mutex);
    CloseHandle(mutex);
    return 0;
}
