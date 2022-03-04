#ifndef IWINDOW_HPP
#define IWINDOW_HPP

#include "Defines.hpp"

class IWindow
{
public:
	enum EventID : uint8_t
	{
		INVALID = 0,
		QUIT = 1
	};

	struct Event
	{
		EventID ID;
	};

public:
	static IWindow* Create(LPCSTR ClassName, LPCSTR WindowName, ULONG Width, ULONG Height);
	static VOID		Destroy(IWindow* pWindow);

	virtual HWND	GetHandle(VOID) = 0;
	virtual BOOL	Open(VOID) = 0;
	virtual BOOL	GetEvent(Event& rEvent) = 0;
};

#endif // IWINDOW_HPP