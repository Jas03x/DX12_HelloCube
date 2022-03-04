#include "CWindow.hpp"

#include <strsafe.h>
#include <windows.h>

#include "console.hpp"

IWindow* IWindow::Create(LPCSTR ClassName, LPCSTR WindowName, ULONG Width, ULONG Height)
{
	return CWindow::Create(ClassName, WindowName, Width, Height);
}

VOID IWindow::Destroy(IWindow* pWindow)
{
	return CWindow::Destroy(static_cast<CWindow*>(pWindow));
}

CWindow* CWindow::Create(LPCSTR ClassName, LPCSTR WindowName, ULONG Width, ULONG Height)
{
	CWindow* pWindow = new CWindow();
	
	if (pWindow->Initialize(ClassName, WindowName, Width, Height) == FALSE)
	{
		CWindow::Destroy(pWindow);
		pWindow = NULL;
	}

	return pWindow;
}

VOID CWindow::Destroy(CWindow* pWindow)
{
	if (pWindow != NULL)
	{
		pWindow->Uninitialize();
		delete pWindow;
	}
}

CWindow::CWindow()
{
	m_hCID = 0;
	m_bOpen = FALSE;

	m_hWnd = NULL;
	m_hInstance = NULL;
	
	ZeroMemory(m_ClassName, sizeof(m_ClassName));
}

CWindow::~CWindow()
{
}

BOOL CWindow::Initialize(LPCSTR ClassName, LPCSTR WindowName, ULONG Width, ULONG Height)
{
	BOOL Status = TRUE;

	m_hInstance = GetModuleHandle(NULL);
	StringCchCopy(m_ClassName, sizeof(m_ClassName), ClassName);

	WNDCLASSEX wndClassEx = { 0 };
	wndClassEx.cbSize = sizeof(WNDCLASSEX);
	wndClassEx.style = CS_HREDRAW | CS_VREDRAW;
	wndClassEx.lpfnWndProc = WindowProcedure;
	wndClassEx.cbClsExtra = 0;
	wndClassEx.cbWndExtra = 0;
	wndClassEx.hInstance = m_hInstance;
	wndClassEx.hIcon = NULL;
	wndClassEx.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndClassEx.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wndClassEx.lpszMenuName = NULL;
	wndClassEx.lpszClassName = ClassName;
	wndClassEx.hIconSm = NULL;

	HWND hDesktopWindow = GetDesktopWindow();

	RECT wndRect = { };
	GetClientRect(hDesktopWindow, &wndRect);
	wndRect.left = ((wndRect.right - wndRect.left) - Width) / 2;
	wndRect.top = ((wndRect.bottom - wndRect.top) - Height) / 2;
	wndRect.right = (wndRect.left + Width);
	wndRect.bottom = (wndRect.top + Height);

	if (AdjustWindowRect(&wndRect, WS_OVERLAPPEDWINDOW, FALSE) != TRUE)
	{
		Status = FALSE;
		Console::Write("Error: Could not calculate window bounds\n");
	}

	if (Status == TRUE)
	{
		m_hCID = RegisterClassEx(&wndClassEx);

		if (m_hCID == 0)
		{
			Status = FALSE;
			Console::Write("Error: Could not register class\n");
		}
	}

	if (Status == TRUE)
	{
		m_hWnd = CreateWindowEx(0, ClassName, WindowName, WS_OVERLAPPEDWINDOW, wndRect.left, wndRect.top, wndRect.right - wndRect.left, wndRect.bottom - wndRect.top, NULL, NULL, m_hInstance, NULL);

		if (m_hWnd == NULL)
		{
			Status = FALSE;
			Console::Write("Error: Could not create window\n");
		}
	}

	if (Status == TRUE)
	{
		SetWindowLongPtr(m_hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
	}

	if (Status == TRUE)
	{
		m_bOpen = TRUE;
		ShowWindow(m_hWnd, SW_SHOW);
	}

	return Status;
}

VOID CWindow::Uninitialize(VOID)
{
	if (m_hWnd != NULL)
	{
		DestroyWindow(m_hWnd);
		m_hWnd = NULL;
	}

	UnregisterClass(m_ClassName, m_hInstance);
}

HWND CWindow::GetHandle(VOID)
{
	return m_hWnd;
}

BOOL CWindow::Open(VOID)
{
	return m_bOpen;
}

LRESULT CWindow::WindowProcedure(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT Result = 0;
	CWindow* pWindow = reinterpret_cast<CWindow*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

	switch (message)
	{
		case WM_CLOSE:
		{
			pWindow->m_bOpen = FALSE;
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

BOOL CWindow::GetEvent(Event& rEvent)
{
	BOOL Status = FALSE;
	MSG msg = { 0 };

	rEvent.ID = EventID::INVALID;

	if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) != FALSE)
	{
		TranslateMessage(&msg);
		DispatchMessageA(&msg);

		if (msg.message == WM_QUIT)
		{
			rEvent.ID = EventID::QUIT;
		}
	}

	return Status;
}
