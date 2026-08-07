#include "ImeStateTip.h"
#include "DllExports.h"

#include <cstdio>
#include <ctime>

// 이 항목("스레드 스코프 컴파트먼트 구독") 전용 임시 로그. 다음 워크리스트 항목(IPC
// 클라이언트)에서 IpcClient_ReportState 호출로 교체되면 이 로그는 제거한다.
static void Log(const wchar_t* message)
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

static void LogValue(const wchar_t* prefix, const VARIANT& value)
{
    wchar_t buf[128];
    if (value.vt == VT_I4)
    {
        swprintf_s(buf, L"%s vt=VT_I4 value=%ld", prefix, value.lVal);
    }
    else
    {
        swprintf_s(buf, L"%s vt=%d (not VT_I4)", prefix, value.vt);
    }
    Log(buf);
}

ImeStateTip::ImeStateTip() : m_refCount(1), m_compartment(nullptr), m_compartmentCookie(TF_INVALID_COOKIE)
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
    else if (IsEqualIID(riid, IID_ITfCompartmentEventSink))
    {
        *ppv = static_cast<ITfCompartmentEventSink*>(this);
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

STDMETHODIMP ImeStateTip::Activate(ITfThreadMgr* threadMgr, TfClientId /* clientId */)
{
    Log(L"Activate() called");
    SubscribeThreadScopeCompartment(threadMgr);
    return S_OK;
}

STDMETHODIMP ImeStateTip::Deactivate()
{
    Log(L"Deactivate() called");
    UnsubscribeThreadScopeCompartment();
    return S_OK;
}

void ImeStateTip::SubscribeThreadScopeCompartment(ITfThreadMgr* threadMgr)
{
    // GetGlobalCompartment()가 아니라 ITfThreadMgr 자신을 ITfCompartmentMgr로 직접 QI —
    // 이 스코프에서만 값이 실제로 관찰되고 OnChange도 온다는 것을 실측으로 확인했다(ADR-0005).
    ITfCompartmentMgr* compartmentMgr = nullptr;
    if (FAILED(threadMgr->QueryInterface(IID_ITfCompartmentMgr, reinterpret_cast<void**>(&compartmentMgr))) || !compartmentMgr)
    {
        Log(L"QI(ITfThreadMgr -> ITfCompartmentMgr) failed");
        return;
    }

    HRESULT hr = compartmentMgr->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, &m_compartment);
    compartmentMgr->Release();
    if (FAILED(hr) || !m_compartment)
    {
        Log(L"GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE) failed");
        m_compartment = nullptr;
        return;
    }

    ReportCurrentValue();

    ITfSource* source = nullptr;
    if (SUCCEEDED(m_compartment->QueryInterface(IID_ITfSource, reinterpret_cast<void**>(&source))) && source)
    {
        source->AdviseSink(IID_ITfCompartmentEventSink, static_cast<ITfCompartmentEventSink*>(this), &m_compartmentCookie);
        source->Release();
    }
}

void ImeStateTip::UnsubscribeThreadScopeCompartment()
{
    if (!m_compartment)
    {
        return;
    }

    if (m_compartmentCookie != TF_INVALID_COOKIE)
    {
        ITfSource* source = nullptr;
        if (SUCCEEDED(m_compartment->QueryInterface(IID_ITfSource, reinterpret_cast<void**>(&source))) && source)
        {
            source->UnadviseSink(m_compartmentCookie);
            source->Release();
        }
        m_compartmentCookie = TF_INVALID_COOKIE;
    }

    m_compartment->Release();
    m_compartment = nullptr;
}

void ImeStateTip::ReportCurrentValue()
{
    if (!m_compartment)
    {
        return;
    }

    VARIANT value;
    VariantInit(&value);
    if (SUCCEEDED(m_compartment->GetValue(&value)))
    {
        LogValue(L"THREADSCOPE value:", value);
    }
    VariantClear(&value);
}

STDMETHODIMP ImeStateTip::OnChange(REFGUID /* rguid */)
{
    Log(L"OnChange() fired");
    ReportCurrentValue();
    return S_OK;
}
