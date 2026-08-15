// FocusHookDll.cpp
//
// ADR-0007: 포커스가 바뀔 때마다 EVENT_SYSTEM_FOREGROUND(WINEVENT_INCONTEXT)로 이 DLL이 그
// 순간 포커스를 얻은 프로세스 안에 실제로 로드된다. TSF가 우리를 Activate()해주길 기다리지
// 않고, 우리가 직접 CoCreateInstance(CLSID_TF_ThreadMgr) -> Activate() ->
// ITfCompartmentMgr QI -> GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE) -> AdviseSink까지
// 건다 — src/ImeIndicatorTip/ImeStateTip.cpp가 8/7까지 정상 동작했던 "스레드 매니저 자신을
// ITfCompartmentMgr로 직접 QI하는"(ADR-0005) 로직과 동일하다. 다른 건 오직 "누가 Activate를
// 부르는가"(TSF 자신 vs 우리가 직접) 뿐이다. prototype/langbar-observation-poc-throwaway
// 브랜치의 LangBarPoc11(LangBarHookDll11.cpp)로 실측 검증됨.
//
// 크래시 이력(fix/focus-hook-crash, 2026-08-15): 위 COM/TSF 작업을 WinEventProc 콜백
// 안에서 직접(동기로) 실행했더니 explorer.exe가 MSCTF.dll 안에서 access violation으로
// 죽었다(Windows Application 로그, faulting module MSCTF.dll, 0xc0000005). MSDN이
// WinEventProc은 최대한 가볍게 하고 무거운 작업은 메시지 큐로 넘기라고 명시한 이유가 바로
// 이것 — 콜백이 실행되는 시점 자체가 TSF 내부 상태 기준으로 재진입 불가 구간일 수 있다.
// 그래서 훅 콜백에서는 스레드 일치 확인까지만 하고, 실제 TSF 작업은 같은 스레드의 다음
// 메시지 디스패치 시점까지 미룬다.
//
// 처음엔 SetTimer(hwnd=nullptr)로 미뤘으나, 실사용 중 최초 1회 최대 ~3초 지연이 관측됐다
// (같은 창에 재포커스하면 즉각 반응 — t_subscribed 가드로 DoTsfInit이 스킵되기 때문에
// 재현되지 않음). WM_TIMER는 메시지 큐가 완전히 빌 때만 합성되는 최저 우선순위 메시지라,
// 포커스 전환 직후처럼 큐가 바쁜 구간에서 계속 밀릴 수 있다는 게 유력한 원인이다. 그래서
// 우리 소유의 숨은 메시지 전용 창(HWND_MESSAGE)을 만들어 PostMessage로 일반 우선순위
// 메시지를 보내는 방식으로 교체했다 — 대상 프로세스(Explorer 등 남의 코드)의 메시지 루프가
// DispatchMessage를 호출하기만 하면 hwnd 기반 라우팅으로 우리 WndProc이 호출된다
// (PostThreadMessage는 hwnd가 없어 DispatchMessage가 아무 데도 못 넘기므로 쓸 수 없다).
// 창은 DoTsfInit이 끝나면 바로 없앤다 — 이후 쓸 데가 없는 채로 남의 프로세스에 정체불명의
// 숨은 창을 영구히 남겨두면 AV/EDR 오탐 표면만 키운다.
//
// 두 번째 크래시(같은 브랜치, 같은 날): 위 두 수정 후에도 Firefox 주소창에서 반복 토글 중
// 크래시가 재현됐다. 이번엔 Firefox 자체 크래시 리포터(Breakpad)의 minidump로 실제 스택을
// 확인 — 크래시 시점 명령어 포인터는 msctf.dll 안(가장 신뢰도 높은 "context" 프레임)이고,
// 그 코드가 읽으려던 주소가 minidump의 unloaded_modules 목록에 있는 ImeFocusHookDll.dll의
// 옛 주소 범위와 정확히 겹쳤다 — 즉 msctf.dll이 AdviseSink로 등록해둔 CompartmentSink(코드가
// 이 DLL 안에 있음)를 호출하려 했는데, 그 시점에 이 DLL이 이미 OS에 의해 언로드돼 있었다.
// WINEVENT_INCONTEXT로 주입된 DLL은 우리가 FreeLibrary를 부르지 않아도 OS가 임의 시점에
// 자동으로 언로드할 수 있다(문서화된 동작) — threadMgr/sink를 "의도적으로 안 놓는" COM
// 참조 카운트 관리만으론 부족했다. DllMain(DLL_PROCESS_ATTACH)에서
// GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN, ...)로 이 DLL 자체를 프로세스 종료
// 때까지 언로드 불가로 고정해 해결했다.
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
// 동등하게 만들기 위함). true는 "구독 완료"뿐 아니라 "구독 시도가 이미 예약/진행 중"도
// 의미한다 — DoTsfInit이 아직 트램폴린 창의 메시지를 기다리는 중일 때 같은 스레드에 또
// 포커스 이벤트가 와도 중복으로 창을 만들지 않기 위함.
static thread_local bool t_subscribed = false;

// 실제 TSF 작업 — WinEventProc 밖(트램폴린 창의 WndProc)에서만 호출된다. 실패 시
// t_subscribed를 되돌려 다음 포커스 전환에서 재시도할 수 있게 한다.
static void DoTsfInit() {
    HRESULT hrInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hrInit) && hrInit != RPC_E_CHANGED_MODE) {
        t_subscribed = false;
        return;
    }
    if (hrInit == RPC_E_CHANGED_MODE) {
        // 이 스레드가 이미 MTA로 초기화돼 있음 — TSF 참여 불가, 건너뛴다.
        t_subscribed = false;
        return;
    }

    ITfThreadMgr* threadMgr = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_ThreadMgr, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_ITfThreadMgr, reinterpret_cast<void**>(&threadMgr));
    if (FAILED(hr) || !threadMgr) {
        t_subscribed = false;
        return;
    }

    TfClientId clientId = 0;
    if (FAILED(threadMgr->Activate(&clientId))) {
        threadMgr->Release();
        t_subscribed = false;
        return;
    }

    ITfCompartmentMgr* compartmentMgr = nullptr;
    hr = threadMgr->QueryInterface(IID_ITfCompartmentMgr,
                                    reinterpret_cast<void**>(&compartmentMgr));
    if (FAILED(hr) || !compartmentMgr) {
        threadMgr->Release();
        t_subscribed = false;
        return;
    }

    ITfCompartment* compartment = nullptr;
    hr = compartmentMgr->GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE, &compartment);
    compartmentMgr->Release();
    if (FAILED(hr) || !compartment) {
        threadMgr->Release();
        t_subscribed = false;
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
    // t_subscribed는 이미 true (FocusHookProc에서 트램폴린 창을 만들 때 미리 설정됨).
}

static HINSTANCE g_hInstance = nullptr;
static const wchar_t* kTrampolineClassName = L"ingee.ImeFocusHook.TsfInitTrampoline";
constexpr UINT WM_INIT_TSF = WM_APP + 1;

static LRESULT CALLBACK TrampolineWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_INIT_TSF) {
        DoTsfInit();
        DestroyWindow(hwnd);  // 부트스트랩 1회용 — 끝나면 바로 없앤다.
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// RegisterClassW는 프로세스 전체에 한 번만 필요하다. 같은 이름으로 두 번 불러도
// ERROR_CLASS_ALREADY_EXISTS만 나고 부작용은 없으므로, 스레드 경합을 피하려 별도
// 동기화 없이 매번 시도 후 결과를 함께 성공으로 취급한다.
static bool EnsureTrampolineClassRegistered() {
    WNDCLASSW wc = {};
    wc.lpfnWndProc = TrampolineWndProc;
    wc.hInstance = g_hInstance;
    wc.lpszClassName = kTrampolineClassName;
    return RegisterClassW(&wc) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

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

    // 이 시점부터 "구독 시도함"으로 표시 — DoTsfInit은 트램폴린 창이 WM_INIT_TSF를 받을 때
    // (이 콜백을 벗어난 뒤) 실행되므로, 그 전에 같은 스레드에 또 포커스 이벤트가 와도 중복
    // 생성을 막는다.
    t_subscribed = true;
    if (!EnsureTrampolineClassRegistered()) {
        t_subscribed = false;
        return;
    }
    HWND trampoline = CreateWindowExW(0, kTrampolineClassName, nullptr, 0, 0, 0, 0, 0,
                                       HWND_MESSAGE, nullptr, g_hInstance, nullptr);
    if (!trampoline || !PostMessageW(trampoline, WM_INIT_TSF, 0, 0)) {
        if (trampoline) DestroyWindow(trampoline);
        t_subscribed = false;
    }
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID) {
    g_hInstance = hinstDLL;
    if (fdwReason == DLL_PROCESS_ATTACH) {
        // WINEVENT_INCONTEXT로 주입된 DLL은 OS가 임의 시점에 자동으로 언로드할 수 있다
        // (우리가 FreeLibrary를 부르지 않아도). 실사용 중 Firefox 크래시로 실측: 언로드된
        // 이 DLL의 옛 주소 범위 안을 msctf.dll이 나중에 읽으려다 access violation
        // (AdviseSink로 등록해둔 CompartmentSink의 코드/vtable이 이 DLL 안에 있는데, DLL이
        // 이미 내려가 있었음). GET_MODULE_HANDLE_EX_FLAG_PIN으로 프로세스 종료 때까지 이
        // DLL이 절대 언로드되지 않도록 고정한다 — FreeLibrary를 몇 번 불러도 안 풀린다.
        // 버그 이력: 처음엔 GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS 없이 호출했다 — 그러면
        // 두 번째 인자(hinstDLL, 사실은 메모리 주소)가 "모듈 이름 문자열"로 잘못 해석돼서
        // 아무 모듈과도 안 맞아 GetModuleHandleExW가 조용히 실패하고(반환값을 안 봐서
        // 못 알아챔) 고정이 전혀 안 됐다. Explorer가 같은 "DLL 언로드 후 dangling 콜백"
        // 크래시로 다시 죽어서 발견 — 이 플래그가 있어야 두 번째 인자를 "이 모듈 안의 한
        // 주소"로 올바르게 해석해 그 주소가 속한 모듈(자기 자신)을 고정한다.
        HMODULE pinned = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                            reinterpret_cast<LPCWSTR>(hinstDLL), &pinned);
    }
    return TRUE;
}
