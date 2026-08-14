// TEMP DIAG (2026-08-09): Notepad 회귀 원인 조사용 스탠드얼론 도구.
// TIP 프로필이 TSF 상에서 실제로 "활성화" 상태인지 IsEnabledLanguageProfile로 직접 조회한다.
// 진단 끝나면 삭제할 것 — 프로젝트 정식 빌드 대상 아님.
#include <windows.h>
#include <msctf.h>
#include <cstdio>

// {381AC302-138C-4A2F-8130-01800D681697}
static const GUID CLSID_ImeIndicatorTip = { 0x381ac302, 0x138c, 0x4a2f, { 0x81, 0x30, 0x01, 0x80, 0x0d, 0x68, 0x16, 0x97 } };
// {15D66B19-64DA-425C-9DF4-8D38467DC292}
static const GUID GUID_ImeIndicatorLanguageProfile = { 0x15d66b19, 0x64da, 0x425c, { 0x9d, 0xf4, 0x8d, 0x38, 0x46, 0x7d, 0xc2, 0x92 } };

int main()
{
    {
        auto printCat = [](const wchar_t* name, const GUID& g) {
            wchar_t s[64];
            StringFromGUID2(g, s, 64);
            wprintf(L"%-40s = %s\n", name, s);
        };
        printCat(L"GUID_TFCAT_CATEGORY_OF_TIP", GUID_TFCAT_CATEGORY_OF_TIP);
        printCat(L"GUID_TFCAT_TIP_KEYBOARD", GUID_TFCAT_TIP_KEYBOARD);
        printCat(L"GUID_TFCAT_TIP_SPEECH", GUID_TFCAT_TIP_SPEECH);
        printCat(L"GUID_TFCAT_TIP_HANDWRITING", GUID_TFCAT_TIP_HANDWRITING);
        printCat(L"GUID_TFCAT_TIPCAP_SECUREMODE", GUID_TFCAT_TIPCAP_SECUREMODE);
        printCat(L"GUID_TFCAT_TIPCAP_UIELEMENTENABLED", GUID_TFCAT_TIPCAP_UIELEMENTENABLED);
        printCat(L"GUID_TFCAT_TIPCAP_INPUTMODECOMPARTMENT", GUID_TFCAT_TIPCAP_INPUTMODECOMPARTMENT);
        printCat(L"GUID_TFCAT_TIPCAP_COMLESS", GUID_TFCAT_TIPCAP_COMLESS);
        printCat(L"GUID_TFCAT_TIPCAP_WOW16", GUID_TFCAT_TIPCAP_WOW16);
        printCat(L"GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT", GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT);
        printCat(L"GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT", GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT);
        printCat(L"GUID_TFCAT_PROP_AUDIODATA", GUID_TFCAT_PROP_AUDIODATA);
        printCat(L"GUID_TFCAT_PROP_INKDATA", GUID_TFCAT_PROP_INKDATA);
        printCat(L"GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER", GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER);
        printCat(L"GUID_TFCAT_DISPLAYATTRIBUTEPROPERTY", GUID_TFCAT_DISPLAYATTRIBUTEPROPERTY);
        wprintf(L"\n--- target: {A028AE76-01B1-46C2-99C4-ACD9858AE02F} ---\n");
    }

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
    {
        printf("CoInitializeEx failed: 0x%08lX\n", hr);
        return 1;
    }

    ITfInputProcessorProfileMgr* profileMgr = nullptr;
    hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
        IID_ITfInputProcessorProfileMgr, reinterpret_cast<void**>(&profileMgr));
    if (FAILED(hr) || !profileMgr)
    {
        printf("CoCreateInstance(ITfInputProcessorProfileMgr) failed: 0x%08lX\n", hr);
        CoUninitialize();
        return 1;
    }

    TF_INPUTPROCESSORPROFILE ourProfile;
    ZeroMemory(&ourProfile, sizeof(ourProfile));
    hr = profileMgr->GetProfile(TF_PROFILETYPE_INPUTPROCESSOR, 0x0412, CLSID_ImeIndicatorTip,
        GUID_ImeIndicatorLanguageProfile, nullptr, &ourProfile);
    printf("GetProfile(ours): hr=0x%08lX dwFlags=0x%08lX (ACTIVE=%d ENABLED=%d)\n",
        hr, ourProfile.dwFlags,
        (ourProfile.dwFlags & TF_IPP_FLAG_ACTIVE) != 0,
        (ourProfile.dwFlags & TF_IPP_FLAG_ENABLED) != 0);

    // 참고용: 0x0412(ko) 언어에 등록된 전체 프로필 열거
    IEnumTfInputProcessorProfiles* enumProfiles = nullptr;
    hr = profileMgr->EnumProfiles(0x0412, &enumProfiles);
    if (SUCCEEDED(hr) && enumProfiles)
    {
        TF_INPUTPROCESSORPROFILE profile;
        ULONG fetched = 0;
        printf("--- 0x0412(ko) 언어에 등록된 전체 프로필 ---\n");
        while (enumProfiles->Next(1, &profile, &fetched) == S_OK && fetched == 1)
        {
            wchar_t clsidStr[64];
            wchar_t profileGuidStr[64];
            StringFromGUID2(profile.clsid, clsidStr, 64);
            StringFromGUID2(profile.guidProfile, profileGuidStr, 64);
            wprintf(L"clsid=%s profile=%s catid=%s dwFlags=0x%08lX (ACTIVE=%d ENABLED=%d)\n",
                clsidStr, profileGuidStr,
                IsEqualGUID(profile.catid, GUID_TFCAT_TIP_KEYBOARD) ? L"KEYBOARD" : L"?",
                profile.dwFlags,
                (profile.dwFlags & TF_IPP_FLAG_ACTIVE) != 0,
                (profile.dwFlags & TF_IPP_FLAG_ENABLED) != 0);
        }
        enumProfiles->Release();
    }
    else
    {
        printf("EnumProfiles failed: 0x%08lX\n", hr);
    }

    // 2026-08-14: 새 CLSID가 AddLanguageProfile만으로 이미 ENABLED=1로 뜨는 것을 확인한 뒤,
    // 오염되지 않은 이 CLSID에서 DeactivateProfile로 ENABLED=0 되돌리면 "한" 표시/Activate()가
    // 회복되는지 깨끗하게 재검증하기 위한 호출.
    printf("\n--- DeactivateProfile로 ENABLED=0 되돌리는 중 ---\n");
    hr = profileMgr->DeactivateProfile(TF_PROFILETYPE_INPUTPROCESSOR, 0x0412, CLSID_ImeIndicatorTip,
        GUID_ImeIndicatorLanguageProfile, nullptr, TF_IPPMF_DISABLEPROFILE);
    printf("DeactivateProfile: hr=0x%08lX\n", hr);

    ZeroMemory(&ourProfile, sizeof(ourProfile));
    hr = profileMgr->GetProfile(TF_PROFILETYPE_INPUTPROCESSOR, 0x0412, CLSID_ImeIndicatorTip,
        GUID_ImeIndicatorLanguageProfile, nullptr, &ourProfile);
    printf("GetProfile(ours, after): hr=0x%08lX dwFlags=0x%08lX (ACTIVE=%d ENABLED=%d)\n",
        hr, ourProfile.dwFlags,
        (ourProfile.dwFlags & TF_IPP_FLAG_ACTIVE) != 0,
        (ourProfile.dwFlags & TF_IPP_FLAG_ENABLED) != 0);

    profileMgr->Release();
    CoUninitialize();
    return 0;
}
