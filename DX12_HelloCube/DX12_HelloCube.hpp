#ifndef DX12_HELLOCUBE
#define DX12_HELLOCUBE

#include "Defines.hpp"

class DX12_HelloCube
{
private:
	class IWindow*	 m_pIWindow;
	class IRenderer* m_pIRenderer;

private:
	DX12_HelloCube(VOID);
	~DX12_HelloCube(VOID);

	BOOL Initialize(VOID);
	VOID Uninitialize(VOID);

private:
	virtual BOOL MainLoop(VOID);

public:
	static BOOL Run(VOID);
};

#endif // DX12_HELLOCUBE