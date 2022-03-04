#ifndef IRENDERER_HPP
#define IRENDERER_HPP

#include "Defines.hpp"

class IRenderer
{
public:
	static IRenderer* Create(HWND hWND, ULONG Width, ULONG Height);
	static VOID		  Destroy(IRenderer* pRenderer);

public:
	virtual BOOL	  Render(VOID) = 0;
};

#endif // IRENDERER_HPP