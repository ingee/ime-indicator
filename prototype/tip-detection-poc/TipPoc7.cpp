// ============================================================================
// TipPoc7.cpp — THROWAWAY PROTOTYPE, 정식 코드가 아님.
//
// TipPoc6에서 확인한 것: ITfThreadMgr::GetGlobalCompartment()를 거치지 않고
// ITfThreadMgr 자신을 ITfCompartmentMgr로 직접 QI해서 얻는 "스레드 스코프"
// GUID_COMPARTMENT_KEYBOARD_OPENCLOSE는 실제로 값이 있고 OnChange도 정상적으로 온다
// (docs/adr 참고). 다만 이건 "이 TIP 인스턴스가 활성화된 스레드"의 상태일 뿐이다.
//
// TipPoc7에서 검증할 것: UI 프로세스가 "지금 포커스된 앱이 A/B/C 중 무엇인가"를
// Win32 GetForegroundWindow()+GetWindowThreadProcessId()로 판단하려고 할 때,
// 그 결과(스레드ID)가 실제로 이 TIP이 GUID_COMPARTMENT_KEYBOARD_OPENCLOSE
// OnChange를 받는 스레드(Activate()에서 받은 스레드)와 일치하는가? 일치해야만
// "포그라운드 판단 결과"로 "어느 TIP 인스턴스의 값을 보여줄지" 고를 수 있다.
// ============================================================================

#include <windows.h>
#include <initguid.h>
#include <msctf.h>
#include <string>
#include <cstdio>
#include <ctime>

// {DB3C1B91-EFEB-4B46-B621-26F605FEEE20}
DEFINE_GUID(CLSID_TipPoc, 0xDB3C1B91, 0xEFEB, 0x4B46, 0xB6, 0x21, 0x26, 0xF6, 0x05, 0xFE, 0xEE, 0x20);
// {DD1C276B-E4E2-43AC-871B-47865DA5530B}
DEFINE_GUID(GUID_TipPocProfile, 0xDD1C276B, 0xE4E2, 0x43AC, 0x87, 0x1B, 0x47, 0x86, 0x5D, 0xA5, 0x53, 0x0B);

static LONG g_DllRefCount = 0;
static HMODULE g_hModule = nullptr;

static const wchar_t* kLogPath = L"C:\\_data\\git\\ime-indicator\\prototype\\tip-detection-poc\\poc-log7.txt";

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
        fwprintf(f, L"[%s] pid=%lu tid=%lu %s\n", timeBuf, GetCurrentProcessId(), GetCurrentThreadId(), msg);
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

// 지금 이 스레드가 실제로 "OS 포그라운드 윈도우를 소유한 스레드"인지 확인하고 로그로 남긴다.
// UI 프로세스가 GetForegroundWindow()+GetWindowThreadProcessId()로 얻는 스레드ID가
// 이 TIP이 활성화된 스레드ID와 같아야만, 포그라운드 판단 결과로 어느 TIP 인스턴스의
// 값을 보여줄지 매칭할 수 있다.
static void LogForegroundThreadMatch(const wchar_t* context)
{
    DWORD myPid = GetCurrentProcessId();
    DWORD myTid = GetCurrentThreadId();

    HWND fg = GetForegroundWindow();
    DWORD fgPid = 0;
    DWORD fgTid = fg ? GetWindowThreadProcessId(fg, &fgPid) : 0;

    wchar_t buf[192];
    swprintf_s(buf, L"%s: myPid=%lu myTid=%lu fgPid=%lu fgTid=%lu match=%s",
        context, myPid, myTid, fgPid, fgTid,
        (fg && fgPid == myPid && fgTid == myTid) ? L"YES" : L"no");
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
        m_threadScopeCompartment(nullptr), m_threadScopeCompartmentCookie(TF_INVALID_COOKIE)
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
        LogForegroundThreadMatch(L"Activate");

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

        // 스레드 스코프 컴파트먼트(TipPoc6에서 확인된 유일하게 동작하는 경로)만 남긴다.
        ITfCompartmentMgr* threadScopeMgr = nullptr;
        if (SUCCEEDED(m_threadMgr->QueryInterface(IID_ITfCompartmentMgr, (void**)&threadScopeMgr)) && threadScopeMgr)
        {
            HRESULT hr = threadScopeMgr->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, &m_threadScopeCompartment);
            threadScopeMgr->Release();
            if (SUCCEEDED(hr) && m_threadScopeCompartment)
            {
                VARIANT v;
                VariantInit(&v);
                if (SUCCEEDED(m_threadScopeCompartment->GetValue(&v)))
                    LogValue(L"THREADSCOPE initial compartment value:", v);
                VariantClear(&v);

                ITfSource* tsource = nullptr;
                if (SUCCEEDED(m_threadScopeCompartment->QueryInterface(IID_ITfSource, (void**)&tsource)))
                {
                    HRESULT hr2 = tsource->AdviseSink(IID_ITfCompartmentEventSink, static_cast<ITfCompartmentEventSink*>(this), &m_threadScopeCompartmentCookie);
                    wchar_t buf3[96];
                    swprintf_s(buf3, L"THREADSCOPE AdviseSink(ITfCompartmentEventSink) hr=0x%08lX cookie=%lu", hr2, m_threadScopeCompartmentCookie);
                    Log(buf3);
                    tsource->Release();
                }
            }
            else
            {
                Log(L"THREADSCOPE GetCompartment failed");
            }
        }
        else
        {
            Log(L"THREADSCOPE QI(ITfThreadMgr -> ITfCompartmentMgr) failed");
        }

        return S_OK;
    }

    STDMETHODIMP Deactivate() override
    {
        Log(L"Deactivate() called.");
        if (m_threadScopeCompartment)
        {
            if (m_threadScopeCompartmentCookie != TF_INVALID_COOKIE)
            {
                ITfSource* tsource = nullptr;
                if (SUCCEEDED(m_threadScopeCompartment->QueryInterface(IID_ITfSource, (void**)&tsource)))
                {
                    tsource->UnadviseSink(m_threadScopeCompartmentCookie);
                    tsource->Release();
                }
            }
            m_threadScopeCompartment->Release();
            m_threadScopeCompartment = nullptr;
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
    STDMETHODIMP OnSetFocus(ITfDocumentMgr*, ITfDocumentMgr*) override
    {
        Log(L"OnSetFocus() fired");
        LogForegroundThreadMatch(L"OnSetFocus");
        return S_OK;
    }
    STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr*) override { return S_OK; }
    STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr*) override { return S_OK; }
    STDMETHODIMP OnPushContext(ITfContext*) override { return S_OK; }
    STDMETHODIMP OnPopContext(ITfContext*) override { return S_OK; }

    // ---- ITfCompartmentEventSink ----
    STDMETHODIMP OnChange(REFGUID rguid) override
    {
        {
            wchar_t guidStr[64];
            StringFromGUID2(rguid, guidStr, 64);
            wchar_t hdr[96];
            swprintf_s(hdr, L"OnChange() fired for %s", guidStr);
            Log(hdr);
        }
        LogForegroundThreadMatch(L"OnChange");
        if (m_threadScopeCompartment)
        {
            VARIANT v;
            VariantInit(&v);
            if (SUCCEEDED(m_threadScopeCompartment->GetValue(&v)))
                LogValue(L"OnChange THREADSCOPE value:", v);
            VariantClear(&v);
        }
        return S_OK;
    }

private:
    LONG m_refCount;
    ITfThreadMgr* m_threadMgr;
    DWORD m_threadMgrCookie;
    ITfCompartment* m_threadScopeCompartment;
    DWORD m_threadScopeCompartmentCookie;
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
    RegSetValueExW(hKey, nullptr, 0, REG_SZ, (const BYTE*)L"TipPoc7 (throwaway prototype)", 60);
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
        const wchar_t* desc = L"TipPoc7 POC";
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
