// FocusHookDll.cpp
//
// ADR-0007: 포커스가 바뀔 때마다 EVENT_SYSTEM_FOREGROUND(WINEVENT_INCONTEXT)로 이 DLL이 그
// 순간 포커스를 얻은 프로세스 안에 실제로 로드된다. TSF가 우리를 Activate()해주길 기다리지
// 않고, 이 콜백 안에서 직접 CoCreateInstance(CLSID_TF_ThreadMgr) -> Activate() ->
// ITfCompartmentMgr QI -> GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE) -> AdviseSink까지
// 건다 — src/ImeIndicatorTip/ImeStateTip.cpp가 8/7까지 정상 동작했던 "스레드 매니저 자신을
// ITfCompartmentMgr로 직접 QI하는"(ADR-0005) 로직과 동일하다. 다른 건 오직 "누가 Activate를
// 부르는가"(TSF 자신 vs 우리가 직접) 뿐이다. prototype/langbar-observation-poc-throwaway
// 브랜치의 LangBarPoc11(LangBarHookDll11.cpp)로 실측 검증됨.
//
// 알려진 한계(ADR-0007 Consequences, 이번 구현 범위 밖):
// - 콜백 스레드가 창 소유 스레드와 다르면(실측상 드문 예외) TSF 작업을 건너뛴다 — 마샬링
//   미구현.
// - threadMgr을 의도적으로 leak시킨다(Release 시 구독이 같이 죽을 가능성 회피) — 프로세스
//   종료 시 OS가 정리한다.

#include <windows.h>
#include <initguid.h>
#include <msctf.h>

#include "IpcClient.h"

class CompartmentSink : public ITfCompartmentEventSink {
public:
    explicit CompartmentSink(ITfCompartment* compartment) : m_compartment(compartment) {
        m_compartment->AddRef();
    }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv) return E_INVALIDARG;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfCompartmentEventSink)) {
            *ppv = static_cast<ITfCompartmentEventSink*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_refCount); }
    STDMETHODIMP_(ULONG) Release() override {
        LONG count = InterlockedDecrement(&m_refCount);
        if (count == 0) delete this;
        return count;
    }

    STDMETHODIMP OnChange(REFGUID) override {
        ReportValue();
        return S_OK;
    }

    void ReportValue() {
        VARIANT v;
        VariantInit(&v);
        HRESULT hr = m_compartment->GetValue(&v);
        if (SUCCEEDED(hr) && v.vt == VT_I4) {
            IpcClient_ReportState(GetCurrentProcessId(), v.lVal != 0);
        }
        VariantClear(&v);
    }

private:
    ~CompartmentSink() { m_compartment->Release(); }
    LONG m_refCount = 1;
    ITfCompartment* m_compartment;
};

// 스레드마다 독립적으로 구독한다 — 같은 프로세스라도 다른 스레드가 나중에 포커스를 얻으면
// 그 스레드도 별도로 구독해야 한다(TSF가 원래 스레드마다 별도로 Activate()를 불러주던 것과
// 동등하게 만들기 위함).
static thread_local bool t_subscribed = false;

extern "C" __declspec(dllexport) void CALLBACK FocusHookProc(HWINEVENTHOOK, DWORD event,
                                                               HWND hwnd, LONG idObject, LONG,
                                                               DWORD, DWORD) {
    if (event != EVENT_SYSTEM_FOREGROUND) return;
    if (idObject != OBJID_WINDOW) return;
    if (t_subscribed) return;

    DWORD windowPid = 0;
    DWORD windowTid = hwnd ? GetWindowThreadProcessId(hwnd, &windowPid) : 0;
    if (windowTid != GetCurrentThreadId()) {
        // 콜백 스레드가 창 소유 스레드와 다름 — 마샬링 미구현, 이번 포커스 전환은 건너뛴다.
        return;
    }

    HRESULT hrInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hrInit) && hrInit != RPC_E_CHANGED_MODE) {
        return;
    }
    if (hrInit == RPC_E_CHANGED_MODE) {
        // 이 스레드가 이미 MTA로 초기화돼 있음 — TSF 참여 불가, 건너뛴다.
        return;
    }

    ITfThreadMgr* threadMgr = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_ThreadMgr, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_ITfThreadMgr, reinterpret_cast<void**>(&threadMgr));
    if (FAILED(hr) || !threadMgr) return;

    TfClientId clientId = 0;
    if (FAILED(threadMgr->Activate(&clientId))) {
        threadMgr->Release();
        return;
    }

    ITfCompartmentMgr* compartmentMgr = nullptr;
    hr = threadMgr->QueryInterface(IID_ITfCompartmentMgr,
                                    reinterpret_cast<void**>(&compartmentMgr));
    if (FAILED(hr) || !compartmentMgr) {
        threadMgr->Release();
        return;
    }

    ITfCompartment* compartment = nullptr;
    hr = compartmentMgr->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, &compartment);
    compartmentMgr->Release();
    if (FAILED(hr) || !compartment) {
        threadMgr->Release();
        return;
    }

    IpcClient_Start();

    CompartmentSink* sink = new CompartmentSink(compartment);
    sink->ReportValue();

    ITfSource* source = nullptr;
    if (SUCCEEDED(compartment->QueryInterface(IID_ITfSource, reinterpret_cast<void**>(&source))) &&
        source) {
        DWORD cookie = TF_INVALID_COOKIE;
        source->AdviseSink(IID_ITfCompartmentEventSink,
                            static_cast<ITfCompartmentEventSink*>(sink), &cookie);
        source->Release();
    }

    compartment->Release();
    // threadMgr을 의도적으로 안 놓는다 — Release하면 이 스레드의 ThreadMgr 자체가
    // 해제되면서 구독도 같이 죽을 수 있다.
    t_subscribed = true;
}

BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) { return TRUE; }
