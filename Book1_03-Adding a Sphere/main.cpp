#include "D3D12Screen.h"
#include "DXRPathTracer.h"
#include "timer.h"

HWND createWindow(const wchar* winTitle, uint width, uint height);

unique_ptr<D3D12Screen> screen;
unique_ptr<DXRPathTracer> tracer;

uint gWidth = 1600;
uint gHeight = 900;
bool minimized = false;

int main()
{
	HWND hwnd = createWindow(L"Book1_03-Adding a Sphere", gWidth, gHeight);
	ShowWindow(hwnd, SW_SHOW);

	tracer = make_unique<DXRPathTracer>(hwnd, gWidth, gHeight);
	screen = make_unique<D3D12Screen>(hwnd, gWidth, gHeight);

	tracer->setupScene();

	double fps, old_fps = 0;
	while (IsWindow(hwnd))
	{
		if (!minimized)
		{
			tracer->update();
			TracedResult trResult = tracer->shootRays();
			screen->display(trResult);
		}

		MSG msg;
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		fps = updateFPS(1.0);
		if (fps != old_fps)
		{
			printf("FPS: %f\n", fps);
			old_fps = fps;
		}
	}

	return 0;
}

LRESULT CALLBACK msgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

HWND createWindow(const wchar* winTitle, uint width, uint height)
{
	WNDCLASS wc = {};
	wc.lpfnWndProc = msgProc;
	wc.hInstance = GetModuleHandle(nullptr);
	wc.lpszClassName = L"RayTracingInDXR_01";
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	RegisterClass(&wc);

	RECT r{ 0, 0, (LONG)width, (LONG)height };
	AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, false);

	HWND hWnd = CreateWindowW(
		L"RayTracingInDXR_01",
		winTitle,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		r.right - r.left,
		r.bottom - r.top,
		nullptr,
		nullptr,
		wc.hInstance,
		nullptr);

	return hWnd;
}

LRESULT CALLBACK msgProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_LBUTTONDOWN:
		tracer->onMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_LBUTTONUP:
		tracer->onMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_MOUSEMOVE:
		tracer->onMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;

	case WM_SIZE:
		if (screen)
		{
			uint width = (uint)LOWORD(lParam);
			uint height = (uint)HIWORD(lParam);
			if (width == 0 || height == 0)
			{
				minimized = true;
				return 0;
			}
			else if (minimized)
			{
				minimized = false;
			}

			tracer->onSizeChanged(width, height);
			screen->onSizeChanged(width, height);
		}
		return 0;
	}

	return DefWindowProc(hwnd, message, wParam, lParam);
}