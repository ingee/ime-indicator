#pragma once

#include <cstdint>

// 훅 DLL에서 UI 프로세스로 (PID, 한/영 상태)를 보고하는 논블로킹 IPC 클라이언트.
// 이 DLL은 포커스 전환마다 임의의 프로세스에 주입되므로, 호출자(컴파트먼트 OnChange 등
// 콜백)를 절대 블록해서는 안 된다 — 실제 파이프 I/O는 전용 워커 스레드에서만 일어난다.
// src/ImeIndicatorTip/IpcClient.h와 프로토콜이 동일한 사본이다(ADR-0007) — UI 쪽
// ImeStateIpcListener.cs와 와이어 포맷을 반드시 맞춰야 한다.
void IpcClient_Start();
void IpcClient_ReportState(uint32_t pid, bool isKoreanOpen);
void IpcClient_Stop();
