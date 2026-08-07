using System.Runtime.InteropServices;
using Vanara.PInvoke;
using static Vanara.PInvoke.MSCTF;

namespace ImeIndicator;

// ITfThreadMgr을 통해 현재 포커스된 문서 관리자(ITfDocumentMgr)의
// GUID_COMPARTMENT_KEYBOARD_OPENCLOSE 컴파트먼트를 조회하는 실제 COM 구현.
internal sealed class TsfCompartmentReader(ITfThreadMgr threadMgr) : ICompartmentReader
{
    public bool? TryReadKeyboardOpen()
    {
        // ITfThreadMgr::GetFocus()는 "이 스레드 매니저 인스턴스" 안에서 포커스된 문서
        // 관리자만 돌려준다. 우리 앱은 스스로 텍스트를 편집하지 않으므로, 아무 조치 없이
        // 호출하면 항상 null이 나온다. 실제로 포커스를 가진(포그라운드) 스레드에 우리
        // 스레드의 입력 상태를 잠깐 붙여야 그 스레드의 포커스 정보를 볼 수 있다.
        uint currentThreadId = NativeMethods.GetCurrentThreadId();
        IntPtr foregroundWindow = NativeMethods.GetForegroundWindow();
        uint foregroundThreadId = NativeMethods.GetWindowThreadProcessId(foregroundWindow, IntPtr.Zero);

        if (foregroundThreadId == 0 || foregroundThreadId == currentThreadId)
        {
            return TryReadFocusedCompartment();
        }

        bool attached = NativeMethods.AttachThreadInput(currentThreadId, foregroundThreadId, true);
        try
        {
            return TryReadFocusedCompartment();
        }
        finally
        {
            if (attached)
            {
                NativeMethods.AttachThreadInput(currentThreadId, foregroundThreadId, false);
            }
        }
    }

    private bool? TryReadFocusedCompartment()
    {
        try
        {
            if (threadMgr.GetFocus() is not ITfCompartmentMgr compartmentMgr)
            {
                return null;
            }

            ITfCompartment compartment = compartmentMgr.GetCompartment(GUID_COMPARTMENT_KEYBOARD_OPENCLOSE);
            object value = compartment.GetValue();
            return value is int rawValue ? rawValue != 0 : null;
        }
        catch (COMException)
        {
            return null;
        }
    }
}
