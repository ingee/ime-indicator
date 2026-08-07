using System.Runtime.InteropServices;
using Vanara.PInvoke;
using static Vanara.PInvoke.MSCTF;

namespace ImeIndicator;

// ITfThreadMgrEventSink로 포커스 전환을 통지받아, 그때마다 이전 컨텍스트의 컴파트먼트
// 구독은 해제하고 새 컨텍스트의 컴파트먼트를 구독한다(ADR-0002). 컴파트먼트 값이 바뀌면
// ITfCompartmentEventSink.OnChange로 통지받아 즉시 상태를 갱신한다. 폴링이 아니라 전부
// TSF 콜백에 의해서만 트리거된다.
internal sealed class TsfImeStateMonitor(ITfThreadMgr threadMgr, IndicatorStateStore stateStore)
    : ITfThreadMgrEventSink, ITfCompartmentEventSink
{
    private ITfCompartment? _advisedCompartment;
    private uint _compartmentCookie;

    private static void Log(string message) =>
        System.IO.File.AppendAllText(@"C:\_tmp\indicator-debug.log", $"[{DateTime.Now:HH:mm:ss.fff}] {message}\n");

    public void Start()
    {
        try
        {
            ((ITfSource)threadMgr).AdviseSink(typeof(ITfThreadMgrEventSink).GUID, this, out uint cookie);
            Log($"AdviseSink(ThreadMgrEventSink) ok, cookie={cookie}");
        }
        catch (COMException ex)
        {
            Log($"AdviseSink(ThreadMgrEventSink) FAILED: {ex.Message} (0x{ex.HResult:X8})");
        }

        // 시작 시 최초 1회, 현재 포커스된 컨텍스트를 조회해 초기 화면을 맞춘다.
        try
        {
            var focus = threadMgr.GetFocus();
            Log($"initial GetFocus() -> {(focus is null ? "null" : focus.GetType().Name)}");
            SubscribeToCompartment(focus);
        }
        catch (COMException ex)
        {
            Log($"initial GetFocus() FAILED: {ex.Message} (0x{ex.HResult:X8})");
        }
    }

    private void SubscribeToCompartment(ITfDocumentMgr? focusedDocument)
    {
        UnsubscribeCompartment();

        try
        {
            if (focusedDocument is not ITfCompartmentMgr compartmentMgr)
            {
                Log($"SubscribeToCompartment: focusedDocument is {(focusedDocument is null ? "null" : "not ITfCompartmentMgr")}");
                return;
            }

            ITfCompartment compartment = compartmentMgr.GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE);
            ((ITfSource)compartment).AdviseSink(typeof(ITfCompartmentEventSink).GUID, this, out uint cookie);
            _advisedCompartment = compartment;
            _compartmentCookie = cookie;
            Log($"SubscribeToCompartment: advised, cookie={cookie}");

            ApplyCompartmentValue(compartment);
        }
        catch (COMException ex)
        {
            Log($"SubscribeToCompartment FAILED: {ex.Message} (0x{ex.HResult:X8})");
        }
    }

    private void UnsubscribeCompartment()
    {
        if (_advisedCompartment is null)
        {
            return;
        }

        try
        {
            ((ITfSource)_advisedCompartment).UnadviseSink(_compartmentCookie);
        }
        catch (COMException)
        {
        }

        _advisedCompartment = null;
    }

    private void ApplyCompartmentValue(ITfCompartment compartment)
    {
        try
        {
            object value = compartment.GetValue();
            Log($"ApplyCompartmentValue: raw={value} (type={value?.GetType().Name})");
            if (value is int rawValue)
            {
                stateStore.Set(rawValue != 0 ? ImeState.Korean : ImeState.English);
            }
        }
        catch (COMException ex)
        {
            Log($"ApplyCompartmentValue FAILED: {ex.Message} (0x{ex.HResult:X8})");
        }
    }

    HRESULT ITfThreadMgrEventSink.OnSetFocus(ITfDocumentMgr? pdimFocus, ITfDocumentMgr? pdimPrevFocus)
    {
        Log("OnSetFocus fired");
        SubscribeToCompartment(pdimFocus);
        return HRESULT.S_OK;
    }

    HRESULT ITfThreadMgrEventSink.OnInitDocumentMgr(ITfDocumentMgr pdim) => HRESULT.S_OK;

    HRESULT ITfThreadMgrEventSink.OnUninitDocumentMgr(ITfDocumentMgr pdim) => HRESULT.S_OK;

    HRESULT ITfThreadMgrEventSink.OnPushContext(ITfContext pic) => HRESULT.S_OK;

    HRESULT ITfThreadMgrEventSink.OnPopContext(ITfContext pic) => HRESULT.S_OK;

    HRESULT ITfCompartmentEventSink.OnChange(in Guid rguid)
    {
        Log("OnChange fired");
        if (_advisedCompartment is not null)
        {
            ApplyCompartmentValue(_advisedCompartment);
        }

        return HRESULT.S_OK;
    }
}
