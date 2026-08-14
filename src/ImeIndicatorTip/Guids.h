#pragma once

#include <windows.h>

// {381AC302-138C-4A2F-8130-01800D681697}
// 2026-08-14: 옛 CLSID({8CD02B2A-...})에 지난 세션들의 실험(ENABLED/ACTIVE 토글 반복 등)으로
// 상태가 오염됐을 가능성을 배제하기 위해 완전히 새 GUID로 교체(TIP Activate() 미호출 회귀 조사).
DEFINE_GUID(CLSID_ImeIndicatorTip, 0x381ac302, 0x138c, 0x4a2f, 0x81, 0x30, 0x01, 0x80, 0x0d, 0x68, 0x16, 0x97);

// {15D66B19-64DA-425C-9DF4-8D38467DC292}
DEFINE_GUID(GUID_ImeIndicatorLanguageProfile, 0x15d66b19, 0x64da, 0x425c, 0x9d, 0xf4, 0x8d, 0x38, 0x46, 0x7d, 0xc2, 0x92);
