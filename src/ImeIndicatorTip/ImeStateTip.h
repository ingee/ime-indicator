#pragma once

#include <windows.h>
#include <msctf.h>

// TSF Text Input Processor. Windows가 텍스트 입력을 다루는 프로세스에 이 DLL을 로드하면서
// Activate()를 호출한다. 지금은 뼈대만 구현 — 스레드 스코프 컴파트먼트 구독(ADR-0005)은
// 다음 워크리스트 항목에서 추가한다.
class ImeStateTip : public ITfTextInputProcessor
{
public:
    ImeStateTip();
    ~ImeStateTip();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // ITfTextInputProcessor
    STDMETHODIMP Activate(ITfThreadMgr* threadMgr, TfClientId clientId) override;
    STDMETHODIMP Deactivate() override;

private:
    LONG m_refCount;
};
