#include "Pch.h"
#include "Tests/PlayWithNoise/PlayWithNoise.h"
#include "DirectX12.h"
#include "Library.h"


static void UpdateFrameTime(HWND window, const char* windowText, double& o_Time, float& o_DeltaTime)
{
	static double s_LastTime = -1.0;
	static double s_LastFpsTime = 0.0;
	static unsigned s_FrameCount = 0;

	if (s_LastTime < 0.0)
	{
		s_LastTime = Lib::GetTime();
		s_LastFpsTime = s_LastTime;
	}

	o_Time = Lib::GetTime();
	o_DeltaTime = (float)(o_Time - s_LastTime);
	s_LastTime = o_Time;

	if ((o_Time - s_LastFpsTime) >= 1.0)
	{
		const double fps = s_FrameCount / (o_Time - s_LastFpsTime);
		const double ms = (1.0 / fps) * 1000.0;
		char text[256];
		snprintf(text, sizeof(text), "[%.1f fps  %.3f ms] %s", fps, ms, windowText);
		SetWindowText(window, text);
		s_LastFpsTime = o_Time;
		s_FrameCount = 0;
	}
	s_FrameCount++;
}

static LRESULT CALLBACK ProcessWindowMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	ImGuiIO& io = ImGui::GetIO();

	switch (message)
	{
	case WM_LBUTTONDOWN:
		io.MouseDown[0] = true;
		return 0;
	case WM_LBUTTONUP:
		io.MouseDown[0] = false;
		return 0;
	case WM_RBUTTONDOWN:
		io.MouseDown[1] = true;
		return 0;
	case WM_RBUTTONUP:
		io.MouseDown[1] = false;
		return 0;
	case WM_MBUTTONDOWN:
		io.MouseDown[2] = true;
		return 0;
	case WM_MBUTTONUP:
		io.MouseDown[2] = false;
		return 0;
	case WM_MOUSEWHEEL:
		io.MouseWheel += GET_WHEEL_DELTA_WPARAM(wparam) > 0 ? 1.0f : -1.0f;
		return 0;
	case WM_MOUSEMOVE:
		io.MousePos.x = (signed short)(lparam);
		io.MousePos.y = (signed short)(lparam >> 16);
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_KEYDOWN:
	{
		if (wparam < 256)
		{
			io.KeysDown[wparam] = true;
			if (wparam == VK_ESCAPE)
				PostQuitMessage(0);
			return 0;
		}
	}
	break;
	case WM_KEYUP:
	{
		if (wparam < 256)
		{
			io.KeysDown[wparam] = false;
			return 0;
		}
	}
	break;
	case WM_CHAR:
	{
		if (wparam > 0 && wparam < 0x10000)
		{
			io.AddInputCharacter((unsigned short)wparam);
			return 0;
		}
	}
	break;
	}
	return DefWindowProc(window, message, wparam, lparam);
}

static HWND MakeWindow(const char* name, uint32_t resolutionX, uint32_t resolutionY)
{
	WNDCLASS winclass = {};
	winclass.lpfnWndProc = ProcessWindowMessage;
	winclass.hInstance = GetModuleHandle(nullptr);
	winclass.hCursor = LoadCursor(nullptr, IDC_ARROW);
	winclass.lpszClassName = name;
	if (!RegisterClass(&winclass))
		assert(0);

	RECT rect = { 0, 0, (int32_t)resolutionX, (int32_t)resolutionY };
	if (!AdjustWindowRect(&rect, WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX, 0))
		assert(0);

	HWND window = CreateWindowEx(
		0, name, name, WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT,
		rect.right - rect.left, rect.bottom - rect.top,
		NULL, NULL, NULL, 0);
	assert(window);

	SetFocus(window);
	SetActiveWindow(window);

	return window;
}

static constexpr char* k_Name = "Demoscene";
static constexpr uint32_t k_Resolution[2] = { 1920, 1080 };

int CALLBACK WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	SetProcessDPIAware();

	HWND window = MakeWindow(k_Name, k_Resolution[0], k_Resolution[1]);

	ImGuiIO& io = ImGui::GetIO();
	io.KeyMap[ImGuiKey_Tab] = VK_TAB;
	io.KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
	io.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
	io.KeyMap[ImGuiKey_UpArrow] = VK_UP;
	io.KeyMap[ImGuiKey_DownArrow] = VK_DOWN;
	io.KeyMap[ImGuiKey_PageUp] = VK_PRIOR;
	io.KeyMap[ImGuiKey_PageDown] = VK_NEXT;
	io.KeyMap[ImGuiKey_Home] = VK_HOME;
	io.KeyMap[ImGuiKey_End] = VK_END;
	io.KeyMap[ImGuiKey_Delete] = VK_DELETE;
	io.KeyMap[ImGuiKey_Backspace] = VK_BACK;
	io.KeyMap[ImGuiKey_Enter] = VK_RETURN;
	io.KeyMap[ImGuiKey_Escape] = VK_ESCAPE;
	io.KeyMap[ImGuiKey_A] = 'A';
	io.KeyMap[ImGuiKey_C] = 'C';
	io.KeyMap[ImGuiKey_V] = 'V';
	io.KeyMap[ImGuiKey_X] = 'X';
	io.KeyMap[ImGuiKey_Y] = 'Y';
	io.KeyMap[ImGuiKey_Z] = 'Z';
	io.ImeWindowHandle = window;
	io.RenderDrawListsFn = nullptr;
	io.DisplaySize = ImVec2((float)k_Resolution[0], (float)k_Resolution[1]);

	ImGui::GetStyle().WindowRounding = 0.0f;


	DirectX12 dx12;
	if (!dx12.Initialize(window))
	{
		return 1;
	}

	TestScene* testScene = new PlayWithNoise(dx12);

	dx12.UploadAndReleaseAllIntermediateResources();

	MSG message = {};
	for (;;)
	{
		if (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&message);
			DispatchMessage(&message);
			if (message.message == WM_QUIT)
				break;
		}
		else
		{
			double frameTime;
			float frameDeltaTime;
			UpdateFrameTime(window, k_Name, frameTime, frameDeltaTime);

			ImGuiIO& io = ImGui::GetIO();
			io.KeyCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
			io.KeyShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
			io.KeyAlt = (GetKeyState(VK_MENU) & 0x8000) != 0;
			io.DeltaTime = frameDeltaTime;

			testScene->Update(frameTime, frameDeltaTime);
			testScene->Draw();

			dx12.Present();
		}
	}

	delete testScene;
	return 0;
}
