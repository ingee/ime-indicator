#include "ImeStateTip.h"
#include "DllExports.h"
#include "IpcClient.h"

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
    IpcClient_Start();
    SubscribeThreadScopeCompartment(threadMgr);
    return S_OK;
}

STDMETHODIMP ImeStateTip::Deactivate()
{
    UnsubscribeThreadScopeCompartment();
    IpcClient_Stop();
    return S_OK;
}

void ImeStateTip::SubscribeThreadScopeCompartment(ITfThreadMgr* threadMgr)
{
    // GetGlobalCompartment()가 아니라 ITfThreadMgr 자신을 ITfCompartmentMgr로 직접 QI —
    // 이 스코프에서만 값이 실제로 관찰되고 OnChange도 온다는 것을 실측으로 확인했다(ADR-0005).
    ITfCompartmentMgr* compartmentMgr = nullptr;
    if (FAILED(threadMgr->QueryInterface(IID_ITfCompartmentMgr, reinterpret_cast<void**>(&compartmentMgr))) || !compartmentMgr)
    {
        return;
    }

    HRESULT hr = compartmentMgr->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, &m_compartment);
    compartmentMgr->Release();
    if (FAILED(hr) || !m_compartment)
    {
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
    if (SUCCEEDED(m_compartment->GetValue(&value)) && value.vt == VT_I4)
    {
        IpcClient_ReportState(GetCurrentProcessId(), value.lVal != 0);
    }
    VariantClear(&value);
}

STDMETHODIMP ImeStateTip::OnChange(REFGUID /* rguid */)
{
    ReportCurrentValue();
    return S_OK;
}
