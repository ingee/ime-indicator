using System.Runtime.InteropServices;

namespace ImeIndicator;

// cmd.exe(ProcessTree상 부모)와 conhost.exe(TSF Compartment의 실제 소유자, 자식) 관계처럼,
// 훅이 보고한 PID가 우리가 관측한 foreground PID의 자식인 경우를 매칭하기 위한 프로세스 트리
// 스냅샷 (.scratch/focus-hook-app-coverage/issues/01-cmd-conhost.md 실측: conhost.exe 프로세스
// 내부에서 GetForegroundWindow()를 부르면 시스템 전역 관측값과 다르게 자기 자신을 가리킨다 —
// UI(GetForegroundWindow 직접 호출)는 cmd.exe를, DLL(conhost.exe 안에서 호출)은 conhost.exe
// 자신을 본다).
internal static class ProcessTree
{
    internal static IReadOnlyDictionary<uint, uint> SnapshotParentPids()
    {
        var result = new Dictionary<uint, uint>();
        IntPtr snapshot = NativeMethods.CreateToolhelp32Snapshot(NativeMethods.TH32CS_SNAPPROCESS, 0);
        if (snapshot == IntPtr.Zero || snapshot == new IntPtr(-1))
        {
            return result;
        }

        try
        {
            var entry = new PROCESSENTRY32 { dwSize = (uint)Marshal.SizeOf<PROCESSENTRY32>() };
            if (NativeMethods.Process32First(snapshot, ref entry))
            {
                do
                {
                    result[entry.th32ProcessID] = entry.th32ParentProcessID;
                } while (NativeMethods.Process32Next(snapshot, ref entry));
            }
        }
        finally
        {
            NativeMethods.CloseHandle(snapshot);
        }

        return result;
    }
}
