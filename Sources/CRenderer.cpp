#include "IRenderer.hpp"
#include "CRenderer.hpp"

#include <windows.h>

#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <d3d12sdklayers.h>

#include <cmath>

#include "Console.hpp"

CONST FLOAT CRenderer::ClearColor[] = { 50.0f / 255.0f, 135.0f / 255.0f, 235.0f / 255.0f, 1.0f };

IRenderer* IRenderer::Create(HWND hWND, ULONG Width, ULONG Height)
{
	return CRenderer::Create(hWND, Width, Height);
}

VOID IRenderer::Destroy(IRenderer* pRenderer)
{
	CRenderer::Destroy(static_cast<CRenderer*>(pRenderer));
}

CRenderer* CRenderer::Create(HWND hWND, ULONG Width, ULONG Height)
{
	CRenderer* pRenderer = new CRenderer();

	if (pRenderer->Initialize(hWND, Width, Height) == FALSE)
	{
		CRenderer::Destroy(pRenderer);
		pRenderer = NULL;
	}

	return pRenderer;
}

VOID CRenderer::Destroy(CRenderer* pRenderer)
{
	if (pRenderer != NULL)
	{
		pRenderer->Uninitialize();
		delete pRenderer;
	}
}

CRenderer::CRenderer()
{
	m_hDxgiDebugModule = NULL;
	m_pfnDxgiGetDebugInterface = NULL;

	m_pIDxgiDebugInterface = NULL;
	m_pIDxgiFactory = NULL;
	m_pIDxgiAdapter = NULL;
	m_pISwapChain = NULL;

	m_pID3D12DebugInterface = NULL;
	m_pIDevice = NULL;
	m_pICommandQueue = NULL;
	m_pIDescriptorHeap = NULL;
	m_pIRenderBuffers[0] = NULL;
	m_pIRenderBuffers[1] = NULL;
	m_pICommandAllocator = NULL;

	m_pIFence = NULL;
	m_hFenceEvent = NULL;
	m_pICommandList = NULL;

	m_FrameIndex = 0;
	m_FenceValue = 0;
	m_DescriptorIncrement = 0;
}

CRenderer::~CRenderer()
{
}

BOOL CRenderer::Initialize(HWND hWND, ULONG Width, ULONG Height)
{
	BOOL Status = TRUE;

	m_hWND = hWND;

	if (D3D12GetDebugInterface(__uuidof(ID3D12Debug), reinterpret_cast<void**>(&m_pID3D12DebugInterface)) == S_OK)
	{
		m_pID3D12DebugInterface->EnableDebugLayer();
	}
	else
	{
		Status = FALSE;
		Console::Write("Error: Failed to get dx12 debug interface\n");
	}

	if (Status == TRUE)
	{
		m_hDxgiDebugModule = GetModuleHandle("dxgidebug.dll");

		if (m_hDxgiDebugModule != NULL)
		{
			m_pfnDxgiGetDebugInterface = reinterpret_cast<HRESULT(*)(RGUID, VOID**)>(GetProcAddress(m_hDxgiDebugModule, "DXGIGetDebugInterface"));

			if (m_pfnDxgiGetDebugInterface == NULL)
			{
				Status = FALSE;
				Console::Write("Error: Could not find function DXGIGetDebugInterface in the dxgi debug module\n");
			}
		}
		else
		{
			Status = FALSE;
			Console::Write("Error: Could not load the dxgi debug module\n");
		}
	}

	if (Status == TRUE)
	{
		if (m_pfnDxgiGetDebugInterface(__uuidof(IDXGIDebug), reinterpret_cast<void**>(&m_pIDxgiDebugInterface)) != S_OK)
		{
			Status = FALSE;
			Console::Write("Error: Failed to get dxgi debug interface\n");
		}
	}

	if (Status == TRUE)
	{
		if (CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, __uuidof(IDXGIFactory7), reinterpret_cast<void**>(&m_pIDxgiFactory)) != S_OK)
		{
			Status = FALSE;
			Console::Write("Error: Failed to create dxgi factory\n");
		}
	}

	if (Status == TRUE)
	{
		Status = EnumerateDxgiAdapters();

		if (Status == FALSE)
		{
			Status = FALSE;
			Console::Write("Error: Could not get dxgi adapter\n");
		}
	}

	if (Status == TRUE)
	{
		D3D12CreateDevice(m_pIDxgiAdapter, D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), reinterpret_cast<void**>(&m_pIDevice));

		if (m_pIDevice == NULL)
		{
			Status = FALSE;
			Console::Write("Error: Could not create a DX12 device\n");
		}
	}

	if (Status == TRUE)
	{
		D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = { };
		cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		cmdQueueDesc.NodeMask = 0;

		if (m_pIDevice->CreateCommandQueue(&cmdQueueDesc, __uuidof(ID3D12CommandQueue), reinterpret_cast<void**>(&m_pICommandQueue)) != S_OK)
		{
			Status = FALSE;
			Console::Write("Error: Failed to create command queue\n");
		}
	}

	if (Status == TRUE)
	{
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { };
		swapChainDesc.Width = Width;
		swapChainDesc.Height = Height;
		swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.Stereo = FALSE;
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = NumBuffers;
		swapChainDesc.Scaling = DXGI_SCALING_NONE;
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
		swapChainDesc.Flags = 0;

		IDXGISwapChain1* pISwapChain1 = NULL;

		if (m_pIDxgiFactory->CreateSwapChainForHwnd(m_pICommandQueue, m_hWND, &swapChainDesc, NULL, NULL, &pISwapChain1) == S_OK)
		{
			pISwapChain1->QueryInterface(__uuidof(IDXGISwapChain4), reinterpret_cast<void**>(&m_pISwapChain));
			pISwapChain1->Release();

			m_FrameIndex = m_pISwapChain->GetCurrentBackBufferIndex();
		}
		else
		{
			Status = FALSE;
			Console::Write("Error: Failed to create swap chain\n");
		}
	}

	if (Status == TRUE)
	{
		D3D12_DESCRIPTOR_HEAP_DESC descHeap = {};
		descHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		descHeap.NumDescriptors = NumBuffers;
		descHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		descHeap.NodeMask = 0;

		if (m_pIDevice->CreateDescriptorHeap(&descHeap, __uuidof(ID3D12DescriptorHeap), reinterpret_cast<void**>(&m_pIDescriptorHeap)) == S_OK)
		{
			m_DescriptorIncrement = m_pIDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		}
		else
		{
			Status = FALSE;
			Console::Write("Error: Failed to create descriptor heap\n");
		}
	}

	if (Status == TRUE)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandle = m_pIDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

		for (UINT i = 0; (Status == TRUE) && (i < NumBuffers); i++)
		{
			if (m_pISwapChain->GetBuffer(i, __uuidof(ID3D12Resource), reinterpret_cast<void**>(&m_pIRenderBuffers[i])) != S_OK)
			{
				Status = FALSE;
				Console::Write("Error: Could not get swap chain buffer %u\n", i);
			}

			m_pIDevice->CreateRenderTargetView(m_pIRenderBuffers[i], NULL, cpuDescHandle);

			cpuDescHandle.ptr += m_DescriptorIncrement;
		}
	}

	if (Status == TRUE)
	{
		if (m_pIDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), reinterpret_cast<void**>(&m_pICommandAllocator)) != S_OK)
		{
			Status = FALSE;
			Console::Write("Error: Could not create command allocator\n");
		}
	}

	if (Status == TRUE)
	{
		if (m_pIDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_pICommandAllocator, NULL, __uuidof(ID3D12GraphicsCommandList), reinterpret_cast<void**>(&m_pICommandList)) == S_OK)
		{
			m_pICommandList->Close();
		}
		else
		{
			Status = FALSE;
			Console::Write("Error: Could not create command list\n");
		}
	}

	if (Status == TRUE)
	{
		if (m_pIDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), reinterpret_cast<void**>(&m_pIFence)) == S_OK)
		{
			m_FenceValue = 1;
		}
		else
		{
			Status = FALSE;
			Console::Write("Error: Could not create fence\n");
		}
	}

	if (Status == TRUE)
	{
		m_hFenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

		if (m_hFenceEvent == NULL)
		{
			Status = FALSE;
			Console::Write("Error: Could not create fence event\n");
		}
	}

	return Status;
}

VOID CRenderer::Uninitialize(VOID)
{
	if (m_hFenceEvent != NULL)
	{
		CloseHandle(m_hFenceEvent);
	}

	if (m_pIFence != NULL)
	{
		m_pIFence->Release();
		m_pIFence = NULL;
	}

	if (m_pICommandList != NULL)
	{
		m_pICommandList->Release();
		m_pICommandList = NULL;
	}

	if (m_pICommandAllocator != NULL)
	{
		m_pICommandAllocator->Release();
		m_pICommandAllocator = NULL;
	}

	for (UINT i = 0; i < NumBuffers; i++)
	{
		if (m_pIRenderBuffers[i] != NULL)
		{
			m_pIRenderBuffers[i]->Release();
			m_pIRenderBuffers[i] = NULL;
		}
	}

	if (m_pIDescriptorHeap != NULL)
	{
		m_pIDescriptorHeap->Release();
		m_pIDescriptorHeap = NULL;
	}

	if (m_pISwapChain != NULL)
	{
		m_pISwapChain->Release();
		m_pISwapChain = NULL;
	}

	if (m_pICommandQueue != NULL)
	{
		m_pICommandQueue->Release();
		m_pICommandQueue = NULL;
	}

	if (m_pIDevice != NULL)
	{
		m_pIDevice->Release();
		m_pIDevice = NULL;
	}

	if (m_pIDxgiAdapter != NULL)
	{
		m_pIDxgiAdapter->Release();
		m_pIDxgiAdapter = NULL;
	}

	if (m_pIDxgiFactory != NULL)
	{
		m_pIDxgiFactory->Release();
		m_pIDxgiFactory = NULL;
	}

	if (m_pIDxgiDebugInterface != NULL)
	{
		m_pIDxgiDebugInterface->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);

		m_pIDxgiDebugInterface->Release();
		m_pIDxgiDebugInterface = NULL;
	}

	if (m_hDxgiDebugModule != NULL)
	{
		FreeLibrary(m_hDxgiDebugModule);

		m_hDxgiDebugModule = NULL;
		m_pfnDxgiGetDebugInterface = NULL;
	}

	if (m_pID3D12DebugInterface != NULL)
	{
		m_pID3D12DebugInterface->Release();
		m_pID3D12DebugInterface = NULL;
	}
}

BOOL CRenderer::PrintAdapterDesc(UINT uIndex, IDXGIAdapter4* pIAdapter)
{
	BOOL Status = TRUE;
	DXGI_ADAPTER_DESC3 Desc = { 0 };

	if (pIAdapter->GetDesc3(&Desc) == S_OK)
	{
		CONST FLOAT GB = 1024.0f * 1024.0f * 1024.0f;

		Console::Write("Adapter %u:\n", uIndex);
		Console::Write("\tDescription: %S\n", Desc.Description);
		Console::Write("\tVendorId: %X\n", Desc.VendorId);
		Console::Write("\tDeviceId: %X\n", Desc.DeviceId);
		Console::Write("\tsubSysId: %X\n", Desc.SubSysId);
		Console::Write("\tRevision: %X\n", Desc.Revision);
		Console::Write("\tDedicatedVideoMemory: %.0f GB\n", ceilf(static_cast<double>(Desc.DedicatedVideoMemory) / GB));
		Console::Write("\tDedicatedSystemMemory: %.0f GB\n", ceilf(static_cast<double>(Desc.DedicatedSystemMemory) / GB));
		Console::Write("\tSharedSystemMemory: %.0f GB\n", ceilf(static_cast<double>(Desc.SharedSystemMemory) / GB));

		if (Desc.Flags != DXGI_ADAPTER_FLAG3_NONE)
		{
			Console::Write("\tFlags:\n");

			if ((Desc.Flags & DXGI_ADAPTER_FLAG3_REMOTE) != 0)
			{
				Console::Write("\t\tRemote\n");
			}

			if ((Desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE) != 0)
			{
				Console::Write("\t\tSoftware\n");
			}

			if ((Desc.Flags & DXGI_ADAPTER_FLAG3_ACG_COMPATIBLE) != 0)
			{
				Console::Write("\t\tACG_Compatible\n");
			}

			if ((Desc.Flags & DXGI_ADAPTER_FLAG3_SUPPORT_MONITORED_FENCES) != 0)
			{
				Console::Write("\t\tSupport_Monitored_Fences\n");
			}

			if ((Desc.Flags & DXGI_ADAPTER_FLAG3_SUPPORT_NON_MONITORED_FENCES) != 0)
			{
				Console::Write("\t\tSupport_Non_Monitored_Fences\n");
			}

			if ((Desc.Flags & DXGI_ADAPTER_FLAG3_KEYED_MUTEX_CONFORMANCE) != 0)
			{
				Console::Write("\t\tKeyed_Mutex_Conformance\n");
			}
		}

		LPCCH GraphicsPreemptionGranularity = "Unknown";
		LPCCH ComputePreemptionGranularity = "Unknown";

		switch (Desc.GraphicsPreemptionGranularity)
		{
			case DXGI_GRAPHICS_PREEMPTION_DMA_BUFFER_BOUNDARY:
				GraphicsPreemptionGranularity = "Dma_Buffer_Boundary";
				break;
			case DXGI_GRAPHICS_PREEMPTION_PRIMITIVE_BOUNDARY:
				GraphicsPreemptionGranularity = "Primitive_Boundary";
				break;
			case DXGI_GRAPHICS_PREEMPTION_TRIANGLE_BOUNDARY:
				GraphicsPreemptionGranularity = "Triangle_Boundary";
				break;
			case DXGI_GRAPHICS_PREEMPTION_PIXEL_BOUNDARY:
				GraphicsPreemptionGranularity = "Pixel_Boundary";
				break;
			case DXGI_GRAPHICS_PREEMPTION_INSTRUCTION_BOUNDARY:
				GraphicsPreemptionGranularity = "Instruction_Boundary";
				break;
		}

		switch (Desc.ComputePreemptionGranularity)
		{
			case DXGI_COMPUTE_PREEMPTION_DMA_BUFFER_BOUNDARY:
				ComputePreemptionGranularity = "Dma_Buffer_Boundary";
				break;
			case DXGI_COMPUTE_PREEMPTION_DISPATCH_BOUNDARY:
				ComputePreemptionGranularity = "Dispatch_Boundary";
				break;
			case DXGI_COMPUTE_PREEMPTION_THREAD_GROUP_BOUNDARY:
				ComputePreemptionGranularity = "Thread_Group_Boundary";
				break;
			case DXGI_COMPUTE_PREEMPTION_THREAD_BOUNDARY:
				ComputePreemptionGranularity = "Thread_Boundary";
				break;
			case DXGI_COMPUTE_PREEMPTION_INSTRUCTION_BOUNDARY:
				ComputePreemptionGranularity = "Instruction_Boundary";
				break;
		}

		Console::Write("\tGraphicsPreemptionBoundary: %s\n", GraphicsPreemptionGranularity);
		Console::Write("\tComputePreemptionGranularity: %s\n", ComputePreemptionGranularity);
	}
	else
	{
		Status = FALSE;
		Console::Write("Error: Could not get description for adapter %u\n", uIndex);
	}

	return Status;
}

BOOL CRenderer::EnumerateDxgiAdapters(VOID)
{
	BOOL Status = TRUE;
	UINT uIndex = 0;
	IDXGIAdapter4* pIAdapter = NULL;
	INT AdapterIndex = -1;

	while (Status == TRUE)
	{
		HRESULT result = m_pIDxgiFactory->EnumAdapterByGpuPreference(uIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, __uuidof(IDXGIAdapter4), reinterpret_cast<void**>(&pIAdapter));

		if (result == DXGI_ERROR_NOT_FOUND)
		{
			break;
		}
		else if (result != S_OK)
		{
			Status = FALSE;
			Console::Write("Error: Could not enumerate adapters\n");
			break;
		}
		else
		{
			Status = PrintAdapterDesc(uIndex, pIAdapter);

			if (Status == TRUE)
			{
				if (AdapterIndex == -1)
				{
					// Check if the adapter supports D3D12 - the result is S_FALSE if the function succeeds but ppDevice is NULL
					if (D3D12CreateDevice(pIAdapter, D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), NULL) == S_FALSE)
					{
						AdapterIndex = uIndex;
						m_pIDxgiAdapter = pIAdapter;
					}
				}
			}

			if (pIAdapter != m_pIDxgiAdapter)
			{
				pIAdapter->Release();
			}

			uIndex++;
		}
	}

	Console::Write("Using adapter %u\n", AdapterIndex);

	return Status;
}

BOOL CRenderer::Render(VOID)
{
	BOOL Status = TRUE;

	if (m_pICommandAllocator->Reset() != S_OK)
	{
		Status = FALSE;
		Console::Write("Error: Failed to reset command allocator\n");
	}

	if (Status == TRUE)
	{
		if (m_pICommandList->Reset(m_pICommandAllocator, NULL) != S_OK)
		{
			Status = FALSE;
			Console::Write("Error: Failed to reset command list\n");
		}
	}

	if (Status == TRUE)
	{
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = m_pIRenderBuffers[m_FrameIndex];
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

		m_pICommandList->ResourceBarrier(1, &barrier);
	}

	if (Status == TRUE)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_pIDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		rtvHandle.ptr += m_FrameIndex * m_DescriptorIncrement;

		m_pICommandList->ClearRenderTargetView(rtvHandle, ClearColor, 0, NULL);
	}

	if (Status == TRUE)
	{
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = m_pIRenderBuffers[m_FrameIndex];
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

		m_pICommandList->ResourceBarrier(1, &barrier);
	}

	if (Status == TRUE)
	{
		if (m_pICommandList->Close() != S_OK)
		{
			Status = FALSE;
			Console::Write("Error: Could not finalize command list\n");
		}
	}

	if (Status == TRUE)
	{
		ID3D12CommandList* pICommandLists[] = { m_pICommandList };
		m_pICommandQueue->ExecuteCommandLists(_countof(pICommandLists), pICommandLists);
	}

	if (Status == TRUE)
	{
		if (m_pISwapChain->Present(1, 0) != S_OK)
		{
			Status = FALSE;
			Console::Write("Error: Failed to present\n");
		}
	}

	if (Status == TRUE)
	{
		if (m_pICommandQueue->Signal(m_pIFence, m_FenceValue) != S_OK)
		{
			Status = FALSE;
			Console::Write("Error: Failed to signal command queue fence\n");
		}
	}

	if (Status == TRUE)
	{
		if (m_pIFence->GetCompletedValue() < m_FenceValue)
		{
			if (m_pIFence->SetEventOnCompletion(m_FenceValue, m_hFenceEvent) == S_OK)
			{
				WaitForSingleObject(m_hFenceEvent, INFINITE);
			}
			else
			{
				Status = FALSE;
				Console::Write("Error: Failed to wait for completion fence\n");
			}
		}

		m_FenceValue++;
	}

	if (Status == TRUE)
	{
		m_FrameIndex = m_pISwapChain->GetCurrentBackBufferIndex();
	}

	return Status;
}
