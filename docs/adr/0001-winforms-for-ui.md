# 인디케이터 UI로 WinForms 사용

인디케이터 창은 단색 배경에 글자 하나만 표시하는 정사각형이며, 항상 위(topmost)·포커스
미획득(no-activate)·툴 윈도우 속성을 가져야 하고, 상시 실행 프로그램이므로 리소스 사용량이
낮아야 한다. WPF의 렌더링 스택(비주얼 트리, DirectX 컴포지션)은 이 정도로 단순한 UI에는
이점이 없고 시작 속도·메모리 오버헤드만 늘린다. 순수 Win32(P/Invoke)는 창 스타일 제어의
자유도는 가장 높지만, 멀티 모니터 열거와 트레이 아이콘을 직접 구현해야 한다. 이에 WinForms를
선택했다: `CreateParams`를 오버라이드해 `WS_EX_NOACTIVATE`/`WS_EX_TOOLWINDOW`를 직접
제어할 수 있고, `Screen.AllScreens`와 `NotifyIcon`이 기본 제공된다.
