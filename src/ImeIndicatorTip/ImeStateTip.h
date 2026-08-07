#pragma once

#include <windows.h>
#include <msctf.h>

// TSF Text Input Processor. Windows가 텍스트 입력을 다루는 프로세스에 이 DLL을 로드하면서
// Activate()를 호출한다. ITfThreadMgr을 GetGlobalCompartment() 없이 직접 ITfCompartmentMgr로
// QueryInterface해서 얻는 "스레드 스코프" GUID_COMPARTMENT_KEYBOARD_OPENCLOSE를 구독한다 —
// 문서/전역/컨텍스트 스코프는 관찰되지 않는다는 것을 실측으로 확인했다(ADR-0005).
// 포커스 전환 감지는 이 TIP의 책임이 아니다 — UI 프로세스가 별도로 OS 포그라운드를
// 추적하고 PID로 매칭한다(ADR-0005).
class ImeStateTip : public ITfTextInputProcessor, public ITfCompartmentEventSink
{
public:
    ImeStateTip();
    ~ImeStateTip();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // ITfTextInputProcessor
    STDMETHODIMP Activate(ITfThreadMgr* threadMgr, TfClientId clientId) override;
    STDMETHODIMP Deactivate() override;

    // ITfCompartmentEventSink
    STDMETHODIMP OnChange(REFGUID rguid) override;

private:
    void SubscribeThreadScopeCompartment(ITfThreadMgr* threadMgr);
    void UnsubscribeThreadScopeCompartment();
    void ReportCurrentValue();

    LONG m_refCount;
    ITfCompartment* m_compartment;
    DWORD m_compartmentCookie;
};
