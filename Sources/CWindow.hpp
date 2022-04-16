#ifndef WINDOW_HPP
#define WINDOW_HPP

#include "CBase.hpp"

#include "IWindow.hpp"

class CWindow : public IWindow, public CBase
{
private:
	HINSTANCE m_hInstance;
	ATOM m_hCID;
	HWND m_hWnd;
	BOOL m_bOpen;
	CHAR m_ClassName[256];

	static LRESULT WindowProcedure(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
	CWindow();
	~CWindow();

	BOOL Initialize(LPCSTR ClassName, LPCSTR WindowName, ULONG Width, ULONG Height);
	VOID Uninitialize(VOID);

public:
	static CWindow* Create(LPCSTR ClassName, LPCSTR WindowName, ULONG Width, ULONG Height);
	static VOID Destroy(CWindow* pWindow);

public:
	virtual HWND GetHandle(VOID);
	virtual BOOL Open(VOID);
	virtual BOOL GetEvent(Event& rEvent);
};

#endif // WINDOW_HPP