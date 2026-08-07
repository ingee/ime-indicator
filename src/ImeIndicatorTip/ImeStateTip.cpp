#include "ImeStateTip.h"
#include "DllExports.h"

#include <cstdio>
#include <ctime>

// 워크리스트 "TIP DLL 뼈대 + 등록/로딩 검증" 항목 전용 임시 로그. 다음 항목(IPC 클라이언트)에서
// IpcClient_ReportState 호출로 교체되면 이 로그는 제거한다.
static void LogActivation(const wchar_t* message)
{
    FILE* file = nullptr;
    if (_wfopen_s(&file, L"C:\\_tmp\\ime-indicator-tip-debug.log", L"a") == 0 && file)
    {
        time_t now = time(nullptr);
        tm localNow;
        localtime_s(&localNow, &now);
        wchar_t timeBuf[32];
        wcsftime(timeBuf, 32, L"%H:%M:%S", &localNow);
        fwprintf(file, L"[%s] pid=%lu %s\n", timeBuf, GetCurrentProcessId(), message);
        fclose(file);
    }
}

ImeStateTip::ImeStateTip() : m_refCount(1)
{
    DllExports_AddDllRef();
}

ImeStateTip::~ImeStateTip()
{
    DllExports_ReleaseDllRef();
}

STDMETHODIMP ImeStateTip::QueryInterface(REFIID riid, void** ppv)
{
    if (!ppv)
    {
        return E_INVALIDARG;
    }

    *ppv = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfTextInputProcessor))
    {
        *ppv = static_cast<ITfTextInputProcessor*>(this);
    }
    else
    {
        return E_NOINTERFACE;
    }

    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) ImeStateTip::AddRef()
{
    return InterlockedIncrement(&m_refCount);
}

STDMETHODIMP_(ULONG) ImeStateTip::Release()
{
    LONG count = InterlockedDecrement(&m_refCount);
    if (count == 0)
    {
        delete this;
    }
    return count;
}

STDMETHODIMP ImeStateTip::Activate(ITfThreadMgr* /* threadMgr */, TfClientId /* clientId */)
{
    LogActivation(L"Activate() called");
    return S_OK;
}

STDMETHODIMP ImeStateTip::Deactivate()
{
    LogActivation(L"Deactivate() called");
    return S_OK;
}
