namespace ImeIndicator;

// TSF COM 호출을 이 인터페이스 뒤로 감춰서, 이후 항목들이 실제 COM 없이 가짜 구현으로
// 상태 전이 로직을 테스트할 수 있게 한다.
internal interface ICompartmentReader
{
    // 포커스된 컨텍스트의 GUID_COMPARTMENT_KEYBOARD_OPENCLOSE 값을 읽는다.
    // 컴파트먼트가 없거나 COM 호출이 실패하면 null.
    bool? TryReadKeyboardOpen();
}
