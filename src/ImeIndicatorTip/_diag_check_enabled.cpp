// TEMP DIAG (2026-08-09): Notepad 회귀 원인 조사용 스탠드얼론 도구.
// TIP 프로필이 TSF 상에서 실제로 "활성화" 상태인지 IsEnabledLanguageProfile로 직접 조회한다.
// 진단 끝나면 삭제할 것 — 프로젝트 정식 빌드 대상 아님.
#include <windows.h>
#include <msctf.h>
#include <cstdio>

// {8CD02B2A-A5A2-4902-A9F7-8ECFCCCF89E2}
static const GUID CLSID_ImeIndicatorTip = { 0x8cd02b2a, 0xa5a2, 0x4902, { 0xa9, 0xf7, 0x8e, 0xcf, 0xcc, 0xcf, 0x89, 0xe2 } };
// {986562BD-97A1-4877-A1FD-082713973899}
static const GUID GUID_ImeIndicatorLanguageProfile = { 0x986562bd, 0x97a1, 0x4877, { 0xa1, 0xfd, 0x08, 0x27, 0x13, 0x97, 0x38, 0x99 } };

int main()
{
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

    printf("\n--- ActivateProfile(TF_IPPMF_ENABLEPROFILE)로 enable 플래그 켜는 중 ---\n");
    hr = profileMgr->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR, 0x0412, CLSID_ImeIndicatorTip,
        GUID_ImeIndicatorLanguageProfile, nullptr, TF_IPPMF_ENABLEPROFILE);
    printf("ActivateProfile: hr=0x%08lX\n", hr);

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
