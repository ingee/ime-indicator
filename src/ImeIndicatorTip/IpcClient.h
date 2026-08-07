#pragma once

#include <cstdint>

// TIP DLL에서 UI 프로세스로 (PID, 한/영 상태)를 보고하는 논블로킹 IPC 클라이언트.
// 이 TIP은 텍스트 입력을 다루는 거의 모든 프로세스에 로드되므로(ADR-0003), 호출자
// (ImeStateTip::OnChange 등 TSF 콜백)를 절대 블록해서는 안 된다 — 실제 파이프 I/O는
// 전용 워커 스레드에서만 일어난다.
void IpcClient_Start();
void IpcClient_ReportState(uint32_t pid, bool isKoreanOpen);
void IpcClient_Stop();
