
#include <Strsafe.h>
#include <Windows.h>

LPCSTR CLASS_NAME  = "DX12HelloCube";
LPCSTR WINDOW_NAME = "DX12 - Hello Cube";

CONST LONG WINDOW_WIDTH  = 512;
CONST LONG WINDOW_HEIGHT = 512;

namespace Memory
{
	HANDLE hHeap = NULL;

	BOOL Initialize(VOID)
	{
		BOOL Status = TRUE;

		hHeap = GetProcessHeap();

		if (hHeap == NULL)
		{
			Status = FALSE;
		}

		return Status;
	}

	VOID Uninitialize(VOID)
	{
		hHeap = NULL;
	}

	PVOID Allocate(SIZE_T nBytes, BOOL bClear)
	{
		DWORD Flags = 0;

		if (bClear)
		{
			Flags |= HEAP_ZERO_MEMORY;
		}

		return HeapAlloc(hHeap, Flags, nBytes);
	}

	BOOL Free(PVOID pMemory)
	{
		BOOL Status = TRUE;

		Status = HeapFree(hHeap, 0, pMemory);

		return Status;
	}
}

namespace Console
{
	HANDLE hStdOut = NULL;
	PCHAR  pBuffer = NULL;
	CONST DWORD MaxLength = 1024;

	BOOL Initialize(VOID)
	{
		BOOL  Status = TRUE;

		hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);

		if ((hStdOut == NULL) || (hStdOut == INVALID_HANDLE_VALUE))
		{
			Status = FALSE;
		}

		if (Status == TRUE)
		{
			pBuffer = reinterpret_cast<PCHAR>(Memory::Allocate(MaxLength, TRUE));
		}

		return Status;
	}

	VOID Uninitialize(VOID)
	{
		hStdOut = NULL;

		if (pBuffer != NULL)
		{
			Memory::Free(pBuffer);
			pBuffer = NULL;
		}
	}

	BOOL Write(LPCCH Msg, ...)
	{
		BOOL Status = TRUE;
		SIZE_T CharsFree = 0;
		SIZE_T CharsUsed = 0;
		DWORD  CharsWritten = 0;

		va_list Args;
		va_start(Args, Msg);

		if (StringCchVPrintfEx(pBuffer, MaxLength, NULL, &CharsFree, 0, Msg, Args) == S_OK)
		{
			CharsUsed = MaxLength - CharsFree;
		}
		else
		{
			Status = FALSE;
		}

		if (Status == TRUE)
		{
			Status = WriteConsole(hStdOut, pBuffer, CharsUsed, &CharsWritten, NULL);
		}

		if (Status == TRUE)
		{
			Status = (CharsWritten == CharsUsed) ? TRUE : FALSE;
		}

		va_end(Args);

		return Status;
	}
}

namespace Window
{
	HINSTANCE hInstance = NULL;
	ATOM hCID = 0;
	HWND hWnd = NULL;
	BOOL bOpen = FALSE;

	LRESULT WindowProcedure(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	BOOL Initialize(VOID)
	{
		BOOL Status = TRUE;

		hInstance = GetModuleHandle(NULL);

		WNDCLASSEX wndClassEx = { 0 };
		wndClassEx.cbSize = sizeof(WNDCLASSEX);
		wndClassEx.style = CS_HREDRAW | CS_VREDRAW;
		wndClassEx.lpfnWndProc = WindowProcedure;
		wndClassEx.cbClsExtra = 0;
		wndClassEx.cbWndExtra = 0;
		wndClassEx.hInstance = hInstance;
		wndClassEx.hIcon = NULL;
		wndClassEx.hCursor = LoadCursor(NULL, IDC_ARROW);
		wndClassEx.hbrBackground = (HBRUSH) GetStockObject(BLACK_BRUSH);
		wndClassEx.lpszMenuName = NULL;
		wndClassEx.lpszClassName = CLASS_NAME;
		wndClassEx.hIconSm = NULL;

		HWND hDesktopWindow = GetDesktopWindow();

		RECT wndRect = { 0 };
		GetClientRect(hDesktopWindow, &wndRect);
		wndRect.left = ((wndRect.right - wndRect.left) - WINDOW_WIDTH) / 2;
		wndRect.top = ((wndRect.bottom - wndRect.top) - WINDOW_HEIGHT) / 2;
		wndRect.right = (wndRect.left + WINDOW_WIDTH);
		wndRect.bottom = (wndRect.top + WINDOW_HEIGHT);

		if (AdjustWindowRect(&wndRect, WS_OVERLAPPEDWINDOW, FALSE) != TRUE)
		{
			Status = FALSE;
			Console::Write("Error: Could not calculate window bounds\n");
		}

		if (Status == TRUE)
		{
			hCID = RegisterClassEx(&wndClassEx);

			if (hCID == 0)
			{
				Status = FALSE;
				Console::Write("Error: Could not register class\n");
			}
		}

		if (Status == TRUE)
		{
			hWnd = CreateWindowEx(0, CLASS_NAME, WINDOW_NAME, WS_OVERLAPPEDWINDOW, wndRect.left, wndRect.top, wndRect.right - wndRect.left, wndRect.bottom - wndRect.top, NULL, NULL, hInstance, NULL);

			if (hWnd == NULL)
			{
				Status = FALSE;
				Console::Write("Error: Could not create window\n");
			}
		}

		if (Status == TRUE)
		{
			bOpen = TRUE;
			ShowWindow(hWnd, SW_SHOW);
		}

		return Status;
	}

	VOID Uninitialize(VOID)
	{
		UnregisterClass(CLASS_NAME, hInstance);
	}

	BOOL Open(VOID)
	{
		return bOpen;
	}

	LRESULT WindowProcedure(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		LRESULT Result = 0;

		switch (message)
		{
			case WM_CLOSE:
			{
				bOpen = FALSE;
				break;
			}

			case WM_DESTROY:
			{
				PostQuitMessage(0);
				break;
			}

			default:
			{
				Result = DefWindowProcA(hWnd, message, wParam, lParam);
				break;
			}
		}

		return Result;
	}
}

BOOL Initialize(VOID)
{
	BOOL Status = TRUE;

	if (Status == TRUE)
	{
		Status = Memory::Initialize();
	}

	if (Status == TRUE)
	{
		Status = Console::Initialize();
	}

	if (Status == TRUE)
	{
		Status = Window::Initialize();
	}

	return Status;
}

VOID Uninitialize(VOID)
{
	Window::Uninitialize();

	Console::Uninitialize();

	Memory::Uninitialize();
}

BOOL Run(VOID)
{
	BOOL Status = TRUE;
	MSG msg = { 0 };

	while (Window::Open())
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) != 0)
		{
			TranslateMessage(&msg);
			DispatchMessageA(&msg);

			if (msg.message == WM_QUIT)
			{
				break;
			}
		}
		else
		{
			// draw
		}
	}

	return Status;
}

INT main(INT ArgC, CHAR* ArgV[])
{
	BOOL Status = TRUE;

	Status = Initialize();

	if (Status == TRUE)
	{
		Run();
	}

	Uninitialize();

	return 0;
}
