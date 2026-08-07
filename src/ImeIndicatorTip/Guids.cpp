// DEFINE_GUID는 INITGUID가 정의된 상태에서 딱 한 번 포함돼야 실제 저장 공간이 생긴다.
// 다른 .cpp 파일들은 Guids.h를 평범하게 include해서 여기서 정의된 심볼을 extern으로 참조한다.
#include <initguid.h>
#include "Guids.h"
