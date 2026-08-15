#include "IpcClient.h"

#include <windows.h>

// UI 프로세스(ImeStateIpcListener)와 이름을 정확히 맞춰야 한다 — 서로 다른 언어라 공유
// 헤더가 불가능한 교차 언어 상수. 시스템에 등록되는 자원 이름은 ingee로 시작한다는 규칙 적용.
static const wchar_t* kPipeName = L"\\\\.\\pipe\\ingee.ImeIndicator.StateReport";

#pragma pack(push, 1)
struct ImeStateMessage
{
    uint32_t pid;
    uint8_t isKoreanOpen;
};
#pragma pack(pop)

static CRITICAL_SECTION g_mailboxLock;
static ImeStateMessage g_mailbox = {};
static bool g_hasPending = false;

static HANDLE g_dataReadyEvent = nullptr;
static HANDLE g_shutdownEvent = nullptr;
static HANDLE g_workerThread = nullptr;
static volatile LONG g_started = 0;

// 연결 → 5바이트 쓰기 → 연결 종료를 한 번만 시도한다. 서버(UI)가 없거나 바쁘면 조용히
// 포기한다 — WaitNamedPipe는 블록될 수 있어 절대 쓰지 않는다. WriteFile도 오버랩드 +
// 짧은 타임아웃으로 걸어서, 서버가 응답 없이 붙잡고 있어도 이 스레드가 무한정 멈추지 않게 한다.
static void SendOneMessage(const ImeStateMessage& message)
{
    HANDLE pipe = CreateFileW(kPipeName, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    if (pipe == INVALID_HANDLE_VALUE)
    {
        return;
    }

    OVERLAPPED overlapped = {};
    overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (overlapped.hEvent)
    {
        DWORD written = 0;
        BOOL ok = WriteFile(pipe, &message, sizeof(message), &written, &overlapped);
        if (!ok && GetLastError() == ERROR_IO_PENDING)
        {
            if (WaitForSingleObject(overlapped.hEvent, 200) != WAIT_OBJECT_0)
            {
                CancelIoEx(pipe, &overlapped);
            }
        }
        CloseHandle(overlapped.hEvent);
    }

    CloseHandle(pipe);
}

static DWORD WINAPI WorkerThreadProc(LPVOID)
{
    HANDLE waitHandles[2] = { g_shutdownEvent, g_dataReadyEvent };
    for (;;)
    {
        DWORD waitResult = WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
        if (waitResult == WAIT_OBJECT_0)
        {
            return 0; // shutdown
        }
        if (waitResult != WAIT_OBJECT_0 + 1)
        {
            continue;
        }

        ImeStateMessage snapshot = {};
        bool hasSnapshot = false;
        EnterCriticalSection(&g_mailboxLock);
        if (g_hasPending)
        {
            snapshot = g_mailbox;
            g_hasPending = false;
            hasSnapshot = true;
        }
        LeaveCriticalSection(&g_mailboxLock);

        if (hasSnapshot)
        {
            SendOneMessage(snapshot);
        }
    }
}

void IpcClient_Start()
{
    if (InterlockedCompareExchange(&g_started, 1, 0) != 0)
    {
        return; // 이미 시작됨 — idempotent
    }

    InitializeCriticalSection(&g_mailboxLock);
    g_dataReadyEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    g_shutdownEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_workerThread = CreateThread(nullptr, 0, WorkerThreadProc, nullptr, 0, nullptr);
}

void IpcClient_ReportState(uint32_t pid, bool isKoreanOpen)
{
    if (!g_started || !g_dataReadyEvent)
    {
        return;
    }

    EnterCriticalSection(&g_mailboxLock);
    g_mailbox.pid = pid;
    g_mailbox.isKoreanOpen = isKoreanOpen ? 1 : 0;
    g_hasPending = true;
    LeaveCriticalSection(&g_mailboxLock);

    SetEvent(g_dataReadyEvent);
}

void IpcClient_Stop()
{
    // 종료 신호만 보내고 워커 스레드 join은 하지 않는다 — 콜백 스레드가
    // 이 때문에 멈추면 안 되기 때문이다.
    if (g_shutdownEvent)
    {
        SetEvent(g_shutdownEvent);
    }
}
