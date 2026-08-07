// ============================================================================
// TipPoc.cpp — THROWAWAY PROTOTYPE, 정식 코드가 아님.
//
// 이 DLL의 목적: TSF Text Input Processor(TIP)로 등록해두면
// (1) 다른 프로세스(메모장 등)에 실제로 로드되어 Activate()가 호출되는지,
// (2) 그 안에서 GUID_COMPARTMENT_KEYBOARD_OPENCLOSE 값을 실제로 읽고
//     변경 이벤트(OnSetFocus/OnChange)를 받을 수 있는지
// 확인하는 것. 실제 IME 기능(글자 조합 등)은 전혀 구현하지 않는다.
// ============================================================================

#include <windows.h>
#include <initguid.h>
#include <msctf.h>
#include <string>
#include <cstdio>
#include <ctime>

// {50B93D70-4FEC-4126-85D5-F1988DF80268}
DEFINE_GUID(CLSID_TipPoc, 0x50B93D70, 0x4FEC, 0x4126, 0x85, 0xD5, 0xF1, 0x98, 0x8D, 0xF8, 0x02, 0x68);
// {34625A7E-1F34-4B03-B84A-6C8255949D46}
DEFINE_GUID(GUID_TipPocProfile, 0x34625A7E, 0x1F34, 0x4B03, 0xB8, 0x4A, 0x6C, 0x82, 0x55, 0x94, 0x9D, 0x46);

static LONG g_DllRefCount = 0;
static HMODULE g_hModule = nullptr;

static const wchar_t* kLogPath = L"C:\\_data\\git\\ime-indicator\\prototype\\tip-detection-poc\\poc-log3.txt";

static void Log(const wchar_t* msg)
{
    FILE* f = nullptr;
    if (_wfopen_s(&f, kLogPath, L"a") == 0 && f)
    {
        time_t t = time(nullptr);
        struct tm tmInfo;
        localtime_s(&tmInfo, &t);
        wchar_t timeBuf[32];
        wcsftime(timeBuf, 32, L"%H:%M:%S", &tmInfo);
        fwprintf(f, L"[%s] pid=%lu %s\n", timeBuf, GetCurrentProcessId(), msg);
        fclose(f);
    }
}

static void LogValue(const wchar_t* prefix, const VARIANT& v)
{
    wchar_t buf[128];
    if (v.vt == VT_I4)
        swprintf_s(buf, L"%s vt=VT_I4 value=%ld", prefix, v.lVal);
    else
        swprintf_s(buf, L"%s vt=%d (not VT_I4)", prefix, v.vt);
    Log(buf);
}

static std::wstring GetModulePathW()
{
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(g_hModule, path, MAX_PATH);
    return path;
}

// ---- ITfTextInputProcessor + ITfThreadMgrEventSink + ITfCompartmentEventSink ----
class TipPoc : public ITfTextInputProcessor, public ITfThreadMgrEventSink, public ITfCompartmentEventSink
{
public:
    TipPoc() : m_refCount(1), m_threadMgr(nullptr), m_threadMgrCookie(TF_INVALID_COOKIE),
        m_compartment(nullptr), m_compartmentCookie(TF_INVALID_COOKIE),
        m_globalCompartment(nullptr), m_globalCompartmentCookie(TF_INVALID_COOKIE),
        m_contextCompartment(nullptr), m_contextCompartmentCookie(TF_INVALID_COOKIE)
    {
        InterlockedIncrement(&g_DllRefCount);
    }
    ~TipPoc() { InterlockedDecrement(&g_DllRefCount); }

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override
    {
        if (!ppv) return E_INVALIDARG;
        *ppv = nullptr;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfTextInputProcessor))
            *ppv = static_cast<ITfTextInputProcessor*>(this);
        else if (IsEqualIID(riid, IID_ITfThreadMgrEventSink))
            *ppv = static_cast<ITfThreadMgrEventSink*>(this);
        else if (IsEqualIID(riid, IID_ITfCompartmentEventSink))
            *ppv = static_cast<ITfCompartmentEventSink*>(this);
        else
            return E_NOINTERFACE;
        AddRef();
        return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_refCount); }
    STDMETHODIMP_(ULONG) Release() override
    {
        LONG c = InterlockedDecrement(&m_refCount);
        if (c == 0) delete this;
        return c;
    }

    // ---- ITfTextInputProcessor ----
    STDMETHODIMP Activate(ITfThreadMgr* ptim, TfClientId) override
    {
        Log(L"Activate() called - loaded and activated in this process.");
        m_threadMgr = ptim;
        m_threadMgr->AddRef();

        ITfSource* source = nullptr;
        if (SUCCEEDED(m_threadMgr->QueryInterface(IID_ITfSource, (void**)&source)))
        {
            HRESULT hr = source->AdviseSink(IID_ITfThreadMgrEventSink, static_cast<ITfThreadMgrEventSink*>(this), &m_threadMgrCookie);
            wchar_t buf[128];
            swprintf_s(buf, L"AdviseSink(ITfThreadMgrEventSink) hr=0x%08lX cookie=%lu", hr, m_threadMgrCookie);
            Log(buf);
            source->Release();
        }

        ITfDocumentMgr* focus = nullptr;
        m_threadMgr->GetFocus(&focus);
        wchar_t buf2[64];
        swprintf_s(buf2, L"initial GetFocus() -> %s", focus ? L"non-null" : L"null");
        Log(buf2);
        SubscribeCompartment(focus);
        if (focus) focus->Release();

        // 비교용: 스레드 매니저의 "전역" 컴파트먼트도 같이 구독해서, 문서 단위 컴파트먼트와
        // 결과가 다른지 비교한다.
        ITfCompartmentMgr* globalMgr = nullptr;
        if (SUCCEEDED(m_threadMgr->GetGlobalCompartment(&globalMgr)) && globalMgr)
        {
            HRESULT hr = globalMgr->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, &m_globalCompartment);
            globalMgr->Release();
            if (SUCCEEDED(hr) && m_globalCompartment)
            {
                VARIANT v;
                VariantInit(&v);
                if (SUCCEEDED(m_globalCompartment->GetValue(&v)))
                    LogValue(L"GLOBAL initial compartment value:", v);
                VariantClear(&v);

                ITfSource* gsource = nullptr;
                if (SUCCEEDED(m_globalCompartment->QueryInterface(IID_ITfSource, (void**)&gsource)))
                {
                    HRESULT hr2 = gsource->AdviseSink(IID_ITfCompartmentEventSink, static_cast<ITfCompartmentEventSink*>(this), &m_globalCompartmentCookie);
                    wchar_t buf3[96];
                    swprintf_s(buf3, L"GLOBAL AdviseSink(ITfCompartmentEventSink) hr=0x%08lX cookie=%lu", hr2, m_globalCompartmentCookie);
                    Log(buf3);
                    gsource->Release();
                }
            }
        }

        return S_OK;
    }

    STDMETHODIMP Deactivate() override
    {
        Log(L"Deactivate() called.");
        UnsubscribeCompartment();
        if (m_globalCompartment)
        {
            if (m_globalCompartmentCookie != TF_INVALID_COOKIE)
            {
                ITfSource* gsource = nullptr;
                if (SUCCEEDED(m_globalCompartment->QueryInterface(IID_ITfSource, (void**)&gsource)))
                {
                    gsource->UnadviseSink(m_globalCompartmentCookie);
                    gsource->Release();
                }
            }
            m_globalCompartment->Release();
            m_globalCompartment = nullptr;
        }
        if (m_threadMgr)
        {
            ITfSource* source = nullptr;
            if (m_threadMgrCookie != TF_INVALID_COOKIE && SUCCEEDED(m_threadMgr->QueryInterface(IID_ITfSource, (void**)&source)))
            {
                source->UnadviseSink(m_threadMgrCookie);
                source->Release();
            }
            m_threadMgr->Release();
            m_threadMgr = nullptr;
        }
        return S_OK;
    }

    // ---- ITfThreadMgrEventSink ----
    STDMETHODIMP OnSetFocus(ITfDocumentMgr* pdimFocus, ITfDocumentMgr*) override
    {
        Log(L"OnSetFocus() fired");
        SubscribeCompartment(pdimFocus);
        return S_OK;
    }
    STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr*) override { return S_OK; }
    STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr*) override { return S_OK; }
    STDMETHODIMP OnPushContext(ITfContext*) override { return S_OK; }
    STDMETHODIMP OnPopContext(ITfContext*) override { return S_OK; }

    // ---- ITfCompartmentEventSink ----
    STDMETHODIMP OnChange(REFGUID rguid) override
    {
        Log(L"OnChange() fired");
        if (m_compartment)
        {
            VARIANT v;
            VariantInit(&v);
            if (SUCCEEDED(m_compartment->GetValue(&v)))
                LogValue(L"OnChange DOC value:", v);
            VariantClear(&v);
        }
        if (m_globalCompartment)
        {
            VARIANT v;
            VariantInit(&v);
            if (SUCCEEDED(m_globalCompartment->GetValue(&v)))
                LogValue(L"OnChange GLOBAL value:", v);
            VariantClear(&v);
        }
        if (m_contextCompartment)
        {
            VARIANT v;
            VariantInit(&v);
            if (SUCCEEDED(m_contextCompartment->GetValue(&v)))
                LogValue(L"OnChange CONTEXT value:", v);
            VariantClear(&v);
        }
        return S_OK;
    }

private:
    void SubscribeCompartment(ITfDocumentMgr* focusedDocument)
    {
        UnsubscribeCompartment();
        if (!focusedDocument) return;

        ITfCompartmentMgr* compartmentMgr = nullptr;
        if (FAILED(focusedDocument->QueryInterface(IID_ITfCompartmentMgr, (void**)&compartmentMgr)) || !compartmentMgr)
        {
            Log(L"SubscribeCompartment: QI to ITfCompartmentMgr failed");
            return;
        }

        ITfCompartment* compartment = nullptr;
        HRESULT hr = compartmentMgr->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, &compartment);
        compartmentMgr->Release();
        if (FAILED(hr) || !compartment)
        {
            Log(L"SubscribeCompartment: GetCompartment failed");
            return;
        }

        VARIANT v;
        VariantInit(&v);
        if (SUCCEEDED(compartment->GetValue(&v)))
            LogValue(L"initial compartment value:", v);
        VariantClear(&v);

        ITfSource* source = nullptr;
        if (SUCCEEDED(compartment->QueryInterface(IID_ITfSource, (void**)&source)))
        {
            HRESULT hr2 = source->AdviseSink(IID_ITfCompartmentEventSink, static_cast<ITfCompartmentEventSink*>(this), &m_compartmentCookie);
            wchar_t buf[96];
            swprintf_s(buf, L"AdviseSink(ITfCompartmentEventSink) hr=0x%08lX cookie=%lu", hr2, m_compartmentCookie);
            Log(buf);
            source->Release();
        }

        m_compartment = compartment; // keep ref (already AddRef'd by GetCompartment)

        // 세 번째 후보: 문서 관리자가 아니라 그 안의 최상단 ITfContext 레벨 컴파트먼트.
        ITfContext* topContext = nullptr;
        if (SUCCEEDED(focusedDocument->GetTop(&topContext)) && topContext)
        {
            ITfCompartmentMgr* ctxCompartmentMgr = nullptr;
            if (SUCCEEDED(topContext->QueryInterface(IID_ITfCompartmentMgr, (void**)&ctxCompartmentMgr)) && ctxCompartmentMgr)
            {
                HRESULT hr3 = ctxCompartmentMgr->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, &m_contextCompartment);
                ctxCompartmentMgr->Release();
                if (SUCCEEDED(hr3) && m_contextCompartment)
                {
                    VARIANT v2;
                    VariantInit(&v2);
                    if (SUCCEEDED(m_contextCompartment->GetValue(&v2)))
                        LogValue(L"CONTEXT initial compartment value:", v2);
                    VariantClear(&v2);

                    ITfSource* csource = nullptr;
                    if (SUCCEEDED(m_contextCompartment->QueryInterface(IID_ITfSource, (void**)&csource)))
                    {
                        HRESULT hr4 = csource->AdviseSink(IID_ITfCompartmentEventSink, static_cast<ITfCompartmentEventSink*>(this), &m_contextCompartmentCookie);
                        wchar_t buf5[96];
                        swprintf_s(buf5, L"CONTEXT AdviseSink(ITfCompartmentEventSink) hr=0x%08lX cookie=%lu", hr4, m_contextCompartmentCookie);
                        Log(buf5);
                        csource->Release();
                    }
                }
            }
            else
            {
                Log(L"SubscribeCompartment: QI ITfContext->ITfCompartmentMgr failed");
            }
            topContext->Release();
        }
        else
        {
            Log(L"SubscribeCompartment: GetTop() failed or returned null");
        }
    }

    void UnsubscribeCompartment()
    {
        if (m_contextCompartment)
        {
            if (m_contextCompartmentCookie != TF_INVALID_COOKIE)
            {
                ITfSource* csource = nullptr;
                if (SUCCEEDED(m_contextCompartment->QueryInterface(IID_ITfSource, (void**)&csource)))
                {
                    csource->UnadviseSink(m_contextCompartmentCookie);
                    csource->Release();
                }
            }
            m_contextCompartment->Release();
            m_contextCompartment = nullptr;
            m_contextCompartmentCookie = TF_INVALID_COOKIE;
        }
        if (!m_compartment) return;
        if (m_compartmentCookie != TF_INVALID_COOKIE)
        {
            ITfSource* source = nullptr;
            if (SUCCEEDED(m_compartment->QueryInterface(IID_ITfSource, (void**)&source)))
            {
                source->UnadviseSink(m_compartmentCookie);
                source->Release();
            }
        }
        m_compartment->Release();
        m_compartment = nullptr;
        m_compartmentCookie = TF_INVALID_COOKIE;
    }

    LONG m_refCount;
    ITfThreadMgr* m_threadMgr;
    DWORD m_threadMgrCookie;
    ITfCompartment* m_compartment;
    DWORD m_compartmentCookie;
    ITfCompartment* m_globalCompartment;
    DWORD m_globalCompartmentCookie;
    ITfCompartment* m_contextCompartment;
    DWORD m_contextCompartmentCookie;
};

// ---- IClassFactory ----
class TipPocFactory : public IClassFactory
{
public:
    TipPocFactory() : m_refCount(1) {}

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override
    {
        if (!ppv) return E_INVALIDARG;
        *ppv = nullptr;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory))
        {
            *ppv = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return InterlockedIncrement(&m_refCount); }
    STDMETHODIMP_(ULONG) Release() override
    {
        LONG c = InterlockedDecrement(&m_refCount);
        if (c == 0) delete this;
        return c;
    }

    STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv) override
    {
        if (pUnkOuter) return CLASS_E_NOAGGREGATION;
        Log(L"ClassFactory::CreateInstance() called.");
        TipPoc* p = new TipPoc();
        HRESULT hr = p->QueryInterface(riid, ppv);
        p->Release();
        return hr;
    }
    STDMETHODIMP LockServer(BOOL fLock) override
    {
        if (fLock) InterlockedIncrement(&g_DllRefCount);
        else InterlockedDecrement(&g_DllRefCount);
        return S_OK;
    }

private:
    LONG m_refCount;
};

// ---- DLL exports ----

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_hModule = hModule;
        Log(L"DllMain(DLL_PROCESS_ATTACH) - DLL loaded into this process.");
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv)
{
    if (!IsEqualCLSID(rclsid, CLSID_TipPoc)) return CLASS_E_CLASSNOTAVAILABLE;
    TipPocFactory* f = new TipPocFactory();
    HRESULT hr = f->QueryInterface(riid, ppv);
    f->Release();
    return hr;
}

STDAPI DllCanUnloadNow()
{
    return g_DllRefCount == 0 ? S_OK : S_FALSE;
}

STDAPI DllRegisterServer()
{
    wchar_t clsidStr[64];
    StringFromGUID2(CLSID_TipPoc, clsidStr, 64);
    std::wstring modulePath = GetModulePathW();

    std::wstring keyPath = std::wstring(L"CLSID\\") + clsidStr;
    HKEY hKey;
    if (RegCreateKeyExW(HKEY_CLASSES_ROOT, keyPath.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) != ERROR_SUCCESS)
        return E_FAIL;
    RegSetValueExW(hKey, nullptr, 0, REG_SZ, (const BYTE*)L"TipPoc (throwaway prototype)", 60);
    RegCloseKey(hKey);

    std::wstring inprocPath = keyPath + L"\\InprocServer32";
    if (RegCreateKeyExW(HKEY_CLASSES_ROOT, inprocPath.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) != ERROR_SUCCESS)
        return E_FAIL;
    RegSetValueExW(hKey, nullptr, 0, REG_SZ, (const BYTE*)modulePath.c_str(), (DWORD)((modulePath.size() + 1) * sizeof(wchar_t)));
    RegSetValueExW(hKey, L"ThreadingModel", 0, REG_SZ, (const BYTE*)L"Apartment", 20);
    RegCloseKey(hKey);

    ITfInputProcessorProfiles* profiles = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
        IID_ITfInputProcessorProfiles, (void**)&profiles);
    if (FAILED(hr)) return hr;

    hr = profiles->Register(CLSID_TipPoc);
    if (SUCCEEDED(hr))
    {
        const wchar_t* desc = L"TipPoc POC";
        hr = profiles->AddLanguageProfile(
            CLSID_TipPoc,
            0x0412, // ko-KR
            GUID_TipPocProfile,
            desc, (ULONG)wcslen(desc),
            modulePath.c_str(), (ULONG)modulePath.size(),
            0);
    }
    profiles->Release();
    return hr;
}

STDAPI DllUnregisterServer()
{
    ITfInputProcessorProfiles* profiles = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
        IID_ITfInputProcessorProfiles, (void**)&profiles);
    if (SUCCEEDED(hr))
    {
        profiles->Unregister(CLSID_TipPoc);
        profiles->Release();
    }

    wchar_t clsidStr[64];
    StringFromGUID2(CLSID_TipPoc, clsidStr, 64);
    std::wstring keyPath = std::wstring(L"CLSID\\") + clsidStr;
    std::wstring inprocPath = keyPath + L"\\InprocServer32";
    RegDeleteKeyW(HKEY_CLASSES_ROOT, inprocPath.c_str());
    RegDeleteKeyW(HKEY_CLASSES_ROOT, keyPath.c_str());
    return S_OK;
}
