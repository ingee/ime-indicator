#include <windows.h>
#include "Guids.h"
#include "ImeStateTip.h"
#include "Registration.h"

static LONG g_dllRefCount = 0;
static HMODULE g_hModule = nullptr;

void DllExports_AddDllRef()
{
    InterlockedIncrement(&g_dllRefCount);
}

void DllExports_ReleaseDllRef()
{
    InterlockedDecrement(&g_dllRefCount);
}

HMODULE DllExports_GetModuleHandle()
{
    return g_hModule;
}

class ImeIndicatorTipClassFactory : public IClassFactory
{
public:
    ImeIndicatorTipClassFactory() : m_refCount(1) {}

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override
    {
        if (!ppv)
        {
            return E_INVALIDARG;
        }

        *ppv = nullptr;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory))
        {
            *ppv = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() override
    {
        return InterlockedIncrement(&m_refCount);
    }

    STDMETHODIMP_(ULONG) Release() override
    {
        LONG count = InterlockedDecrement(&m_refCount);
        if (count == 0)
        {
            delete this;
        }
        return count;
    }

    STDMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** ppv) override
    {
        if (outer)
        {
            return CLASS_E_NOAGGREGATION;
        }

        ImeStateTip* tip = new ImeStateTip();
        HRESULT hr = tip->QueryInterface(riid, ppv);
        tip->Release();
        return hr;
    }

    STDMETHODIMP LockServer(BOOL lock) override
    {
        if (lock)
        {
            InterlockedIncrement(&g_dllRefCount);
        }
        else
        {
            InterlockedDecrement(&g_dllRefCount);
        }
        return S_OK;
    }

private:
    LONG m_refCount;
};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_hModule = hModule;
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void** ppv)
{
    if (!IsEqualCLSID(clsid, CLSID_ImeIndicatorTip))
    {
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    ImeIndicatorTipClassFactory* factory = new ImeIndicatorTipClassFactory();
    HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    return hr;
}

STDAPI DllCanUnloadNow()
{
    return g_dllRefCount == 0 ? S_OK : S_FALSE;
}

STDAPI DllRegisterServer()
{
    return ImeIndicatorTip_Register(g_hModule);
}

STDAPI DllUnregisterServer()
{
    return ImeIndicatorTip_Unregister();
}
