// DEFINE_GUID는 INITGUID가 정의된 상태에서 딱 한 번 포함돼야 실제 저장 공간이 생긴다.
// 다른 .cpp 파일들은 Guids.h/msctf.h를 평범하게 include해서 여기서 정의된 심볼을 extern으로
// 참조한다. msctf.h도 여기서 함께 include해서 GUID_COMPARTMENT_KEYBOARD_OPENCLOSE 등
// TSF GUID들의 저장 공간도 이 한 파일에서 만든다(uuid.lib가 커버하는지 불확실하므로 안전하게).
#include <initguid.h>
#include <msctf.h>
#include "Guids.h"
