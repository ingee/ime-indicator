#include "Registration.h"
#include "Guids.h"

#include <msctf.h>
#include <string>

// 실측 결과: CLSID를 HKEY_CURRENT_USER\Software\Classes에만 등록하면 COM 자체(CoCreateInstance)는
// 정상 동작하지만, TSF는 이 TIP을 새로 뜨는 프로세스에 로드하지 않는다(확인 방법: 등록 후
// 메모장을 전부 종료했다가 완전히 새로 띄워도 Activate()가 호출되지 않음). HKEY_LOCAL_MACHINE에
// 등록해야 실제로 로드된다 — 즉 이 TIP의 설치 단계는 관리자 권한이 필요하다. docs/ui-spec.md/
// ADR-0005가 미검증으로 남겨뒀던 질문의 답이 이걸로 확정됐다.
static const wchar_t* kDisplayName = L"ingee.ImeIndicatorTip";
static const LANGID kKoreanLangId = 0x0412;

static std::wstring GetModulePathW(HMODULE moduleHandle)
{
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(moduleHandle, path, MAX_PATH);
    return path;
}

HRESULT ImeIndicatorTip_Register(HMODULE moduleHandle)
{
    wchar_t clsidStr[64];
    StringFromGUID2(CLSID_ImeIndicatorTip, clsidStr, 64);
    std::wstring modulePath = GetModulePathW(moduleHandle);

    std::wstring clsidKeyPath = std::wstring(L"Software\\Classes\\CLSID\\") + clsidStr;
    HKEY clsidKey;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, clsidKeyPath.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &clsidKey, nullptr) != ERROR_SUCCESS)
    {
        return E_FAIL;
    }
    RegSetValueExW(clsidKey, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(kDisplayName),
        static_cast<DWORD>((wcslen(kDisplayName) + 1) * sizeof(wchar_t)));
    RegCloseKey(clsidKey);

    std::wstring inprocKeyPath = clsidKeyPath + L"\\InprocServer32";
    HKEY inprocKey;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, inprocKeyPath.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &inprocKey, nullptr) != ERROR_SUCCESS)
    {
        return E_FAIL;
    }
    RegSetValueExW(inprocKey, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(modulePath.c_str()),
        static_cast<DWORD>((modulePath.size() + 1) * sizeof(wchar_t)));
    RegSetValueExW(inprocKey, L"ThreadingModel", 0, REG_SZ, reinterpret_cast<const BYTE*>(L"Apartment"), 20);
    RegCloseKey(inprocKey);

    ITfInputProcessorProfiles* profiles = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
        IID_ITfInputProcessorProfiles, reinterpret_cast<void**>(&profiles));
    if (FAILED(hr))
    {
        return hr;
    }

    hr = profiles->Register(CLSID_ImeIndicatorTip);
    if (SUCCEEDED(hr))
    {
        hr = profiles->AddLanguageProfile(
            CLSID_ImeIndicatorTip,
            kKoreanLangId,
            GUID_ImeIndicatorLanguageProfile,
            kDisplayName, static_cast<ULONG>(wcslen(kDisplayName)),
            modulePath.c_str(), static_cast<ULONG>(modulePath.size()),
            0);
    }
    profiles->Release();
    if (FAILED(hr))
    {
        return hr;
    }

    // GUID_TFCAT_TIP_KEYBOARD 카테고리 등록 — 이게 없으면 일부 호스트(Excel 등)가 이 TIP을
    // 아예 로드하지 않는 것을 실측으로 확인했다. 메모장/터미널처럼 단순한 호스트는 이
    // 카테고리 없이도 로드했지만, Office는 더 까다롭게 검사하는 것으로 보인다.
    ITfCategoryMgr* categoryMgr = nullptr;
    hr = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
        IID_ITfCategoryMgr, reinterpret_cast<void**>(&categoryMgr));
    if (SUCCEEDED(hr))
    {
        hr = categoryMgr->RegisterCategory(CLSID_ImeIndicatorTip, GUID_TFCAT_TIP_KEYBOARD, CLSID_ImeIndicatorTip);
        categoryMgr->Release();
    }
    return hr;
}

HRESULT ImeIndicatorTip_Unregister()
{
    ITfCategoryMgr* categoryMgr = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
        IID_ITfCategoryMgr, reinterpret_cast<void**>(&categoryMgr))))
    {
        categoryMgr->UnregisterCategory(CLSID_ImeIndicatorTip, GUID_TFCAT_TIP_KEYBOARD, CLSID_ImeIndicatorTip);
        categoryMgr->Release();
    }

    ITfInputProcessorProfiles* profiles = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
        IID_ITfInputProcessorProfiles, reinterpret_cast<void**>(&profiles));
    if (SUCCEEDED(hr))
    {
        profiles->Unregister(CLSID_ImeIndicatorTip);
        profiles->Release();
    }

    wchar_t clsidStr[64];
    StringFromGUID2(CLSID_ImeIndicatorTip, clsidStr, 64);
    std::wstring clsidKeyPath = std::wstring(L"Software\\Classes\\CLSID\\") + clsidStr;

    // RegDeleteKeyW는 서브키가 남아있으면 실패한다(InprocServer32를 먼저 지워야 함).
    // RegDeleteTreeW는 키와 그 아래 전부를 한 번에 재귀적으로 지워서 순서 문제가 없다.
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, clsidKeyPath.c_str());
    RegDeleteKeyW(HKEY_LOCAL_MACHINE, clsidKeyPath.c_str());
    return S_OK;
}
