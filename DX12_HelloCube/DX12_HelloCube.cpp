#include "DX12_HelloCube.hpp"

#include "Config.hpp"

#include "Console.hpp"
#include "Memory.hpp"
#include "IWindow.hpp"
#include "IRenderer.hpp"

BOOL DX12_HelloCube::Run(VOID)
{
	BOOL Status = TRUE;
	
	DX12_HelloCube App;
	
	if (App.Initialize() != TRUE)
	{
		Status = FALSE;
	}

	if (Status == TRUE)
	{
		Status = App.MainLoop();
	}
	
	App.Uninitialize();

	return Status;
}

DX12_HelloCube::DX12_HelloCube(VOID)
{
	m_pIWindow = NULL;
	m_pIRenderer = NULL;
}

DX12_HelloCube::~DX12_HelloCube(VOID)
{

}

BOOL DX12_HelloCube::Initialize(VOID)
{
	BOOL Status = TRUE;

	if (Status == TRUE)
	{
		if (Memory::Initialize() != TRUE)
		{
			Status = FALSE;
		}
	}

	if (Status == TRUE)
	{
		if (Console::Initialize() != TRUE)
		{
			Status = FALSE;
		}
	}

	if (Status == TRUE)
	{
		m_pIWindow = IWindow::Create(CLASS_NAME, WINDOW_NAME, WINDOW_WIDTH, WINDOW_HEIGHT);
		if (m_pIWindow == NULL)
		{
			Status = FALSE;
		}
	}

	if (Status == TRUE)
	{
		m_pIRenderer = IRenderer::Create(m_pIWindow->GetHandle(), WINDOW_WIDTH, WINDOW_HEIGHT);
		if (m_pIRenderer == NULL)
		{
			Status = FALSE;
		}
	}

	return Status;
}

VOID DX12_HelloCube::Uninitialize(VOID)
{
	if (m_pIRenderer != NULL)
	{
		IRenderer::Destroy(m_pIRenderer);
		m_pIRenderer = NULL;
	}

	if (m_pIWindow != NULL)
	{
		IWindow::Destroy(m_pIWindow);
		m_pIWindow = NULL;
	}

	Console::Uninitialize();

	Memory::Uninitialize();
}

BOOL DX12_HelloCube::MainLoop(VOID)
{
	BOOL Status = TRUE;
	
	IWindow::Event Event = { };

	while ((Status == TRUE) && m_pIWindow->Open())
	{
		if (m_pIWindow->GetEvent(Event) == TRUE)
		{
			if (Event.ID == IWindow::EventID::QUIT)
			{
				break;
			}
		}
		else
		{
			m_pIRenderer->Render();
		}
	}

	return Status;
}
