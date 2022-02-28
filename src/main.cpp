
#include <Strsafe.h>
#include <Windows.h>

#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <d3d12sdklayers.h>

#include <cmath>

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
		if (hWnd != NULL)
		{
			DestroyWindow(hWnd);
			hWnd = NULL;
		}

		UnregisterClass(CLASS_NAME, hInstance);
	}

	HWND GetHandle()
	{
		return hWnd;
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

namespace Renderer
{
	CONST UINT					NumBuffers = 2;

	HMODULE                     hDxgiDebugModule = NULL;
	HRESULT                     (*pfnDxgiGetDebugInterface)(REFIID, VOID**);

	IDXGIDebug*					pIDxgiDebugInterface = NULL;
	IDXGIFactory7*				pIDxgiFactory = NULL;
	IDXGIAdapter4*				pIDxgiAdapter = NULL;
	IDXGISwapChain4*			pISwapChain = NULL;

	ID3D12Debug*				pID3D12DebugInterface = NULL;
	ID3D12Device*				pIDevice = NULL;
	ID3D12CommandQueue*			pICommandQueue = NULL;
	ID3D12DescriptorHeap*		pIDescriptorHeap = NULL;
	ID3D12Resource*				pIRenderBuffers[NumBuffers] = { NULL, NULL };
	ID3D12CommandAllocator*		pICommandAllocator = NULL;
	
	ID3D12Fence*				pIFence = NULL;
	HANDLE						hFenceEvent = NULL;
	ID3D12GraphicsCommandList*  pICommandList = NULL;

	UINT						FrameIndex = 0;
	UINT64                      FenceValue = 0;
	SIZE_T						DescriptorIncrement = 0;

	CONST FLOAT                 ClearColor[] = { 50.0f / 255.0f, 135.0f / 255.0f, 235.0f / 255.0f, 1.0f };

	BOOL						EnumerateDxgiAdapters(VOID);

	BOOL Initialize(VOID)
	{
		BOOL Status = TRUE;

		if (D3D12GetDebugInterface(__uuidof(ID3D12Debug), reinterpret_cast<void**>(&pID3D12DebugInterface)) == S_OK)
		{
			pID3D12DebugInterface->EnableDebugLayer();
		}
		else
		{
			Status = FALSE;
			Console::Write("Error: Failed to get dx12 debug interface\n");
		}

		if (Status == TRUE)
		{
			hDxgiDebugModule = GetModuleHandle("dxgidebug.dll");

			if (hDxgiDebugModule != NULL)
			{
				pfnDxgiGetDebugInterface = reinterpret_cast<HRESULT(*)(REFIID, VOID**)>(GetProcAddress(hDxgiDebugModule, "DXGIGetDebugInterface"));

				if (pfnDxgiGetDebugInterface == NULL)
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
			if (pfnDxgiGetDebugInterface(__uuidof(IDXGIDebug), reinterpret_cast<void**>(&pIDxgiDebugInterface)) != S_OK)
			{
				Status = FALSE;
				Console::Write("Error: Failed to get dxgi debug interface\n");
			}
		}

		if (Status == TRUE)
		{
			if (CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, __uuidof(IDXGIFactory7), reinterpret_cast<void**>(&pIDxgiFactory)) != S_OK)
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
			D3D12CreateDevice(pIDxgiAdapter, D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), reinterpret_cast<void**>(&pIDevice));

			if (pIDevice == NULL)
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

			if (pIDevice->CreateCommandQueue(&cmdQueueDesc, __uuidof(ID3D12CommandQueue), reinterpret_cast<void**>(&pICommandQueue)) != S_OK)
			{
				Status = FALSE;
				Console::Write("Error: Failed to create command queue\n");
			}
		}

		if (Status == TRUE)
		{
			DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { };
			swapChainDesc.Width = WINDOW_WIDTH;
			swapChainDesc.Height = WINDOW_HEIGHT;
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

			if (pIDxgiFactory->CreateSwapChainForHwnd(pICommandQueue, Window::GetHandle(), &swapChainDesc, NULL, NULL, &pISwapChain1) == S_OK)
			{
				pISwapChain1->QueryInterface(__uuidof(IDXGISwapChain4), reinterpret_cast<void**>(&pISwapChain));
				pISwapChain1->Release();

				FrameIndex = pISwapChain->GetCurrentBackBufferIndex();
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

			if (pIDevice->CreateDescriptorHeap(&descHeap, __uuidof(ID3D12DescriptorHeap), reinterpret_cast<void**>(&pIDescriptorHeap)) == S_OK)
			{
				DescriptorIncrement = pIDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			}
			else
			{
				Status = FALSE;
				Console::Write("Error: Failed to create descriptor heap\n");
			}
		}

		if (Status == TRUE)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE cpuDescHandle = pIDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

			for (UINT i = 0; (Status == TRUE) && (i < NumBuffers); i++)
			{
				if (pISwapChain->GetBuffer(i, __uuidof(ID3D12Resource), reinterpret_cast<void**>(&pIRenderBuffers[i])) != S_OK)
				{
					Status = FALSE;
					Console::Write("Error: Could not get swap chain buffer %u\n", i);
				}

				pIDevice->CreateRenderTargetView(pIRenderBuffers[i], NULL, cpuDescHandle);

				cpuDescHandle.ptr += DescriptorIncrement;
			}
		}

		if (Status == TRUE)
		{
			if (pIDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), reinterpret_cast<void**>(&pICommandAllocator)) != S_OK)
			{
				Status = FALSE;
				Console::Write("Error: Could not create command allocator\n");
			}
		}

		if (Status == TRUE)
		{
			if (pIDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pICommandAllocator, NULL, __uuidof(ID3D12GraphicsCommandList), reinterpret_cast<void**>(&pICommandList)) == S_OK)
			{
				pICommandList->Close();
			}
			else
			{
				Status = FALSE;
				Console::Write("Error: Could not create command list\n");
			}
		}

		if (Status == TRUE)
		{
			if (pIDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), reinterpret_cast<void**>(&pIFence)) == S_OK)
			{
				FenceValue = 1;
			}
			else
			{
				Status = FALSE;
				Console::Write("Error: Could not create fence\n");
			}
		}

		if (Status == TRUE)
		{
			hFenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

			if (hFenceEvent == NULL)
			{
				Status = FALSE;
				Console::Write("Error: Could not create fence event\n");
			}
		}

		return Status;
	}

	VOID Uninitialize(VOID)
	{
		if (hFenceEvent != NULL)
		{
			CloseHandle(hFenceEvent);
		}

		if (pIFence != NULL)
		{
			pIFence->Release();
			pIFence = NULL;
		}

		if (pICommandList != NULL)
		{
			pICommandList->Release();
			pICommandList = NULL;
		}

		if (pICommandAllocator != NULL)
		{
			pICommandAllocator->Release();
			pICommandAllocator = NULL;
		}

		for (UINT i = 0; i < NumBuffers; i++)
		{
			if (pIRenderBuffers[i] != NULL)
			{
				pIRenderBuffers[i]->Release();
				pIRenderBuffers[i] = NULL;
			}
		}

		if (pIDescriptorHeap != NULL)
		{
			pIDescriptorHeap->Release();
			pIDescriptorHeap = NULL;
		}

		if (pISwapChain != NULL)
		{
			pISwapChain->Release();
			pISwapChain = NULL;
		}

		if (pICommandQueue != NULL)
		{
			pICommandQueue->Release();
			pICommandQueue = NULL;
		}

		if (pIDevice != NULL)
		{
			pIDevice->Release();
			pIDevice = NULL;
		}

		if (pIDxgiAdapter != NULL)
		{
			pIDxgiAdapter->Release();
			pIDxgiAdapter = NULL;
		}

		if (pIDxgiFactory != NULL)
		{
			pIDxgiFactory->Release();
			pIDxgiFactory = NULL;
		}

		if (pIDxgiDebugInterface != NULL)
		{
			pIDxgiDebugInterface->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);

			pIDxgiDebugInterface->Release();
			pIDxgiDebugInterface = NULL;
		}

		if (hDxgiDebugModule != NULL)
		{
			FreeLibrary(hDxgiDebugModule);

			hDxgiDebugModule = NULL;
			pfnDxgiGetDebugInterface = NULL;
		}

		if (pID3D12DebugInterface != NULL)
		{
			pID3D12DebugInterface->Release();
			pID3D12DebugInterface = NULL;
		}
	}

	BOOL PrintAdapterDesc(UINT uIndex, IDXGIAdapter4* pIAdapter)
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

	BOOL EnumerateDxgiAdapters(VOID)
	{
		BOOL Status = TRUE;
		UINT uIndex = 0;
		IDXGIAdapter4* pIAdapter = NULL;
		INT AdapterIndex = -1;

		while (Status == TRUE)
		{
			HRESULT result = pIDxgiFactory->EnumAdapterByGpuPreference(uIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, __uuidof(IDXGIAdapter4), reinterpret_cast<void**>(&pIAdapter));

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
							pIDxgiAdapter = pIAdapter;
						}
					}
				}

				if (pIAdapter != pIDxgiAdapter)
				{
					pIAdapter->Release();
				}

				uIndex++;
			}
		}

		Console::Write("Using adapter %u\n", AdapterIndex);

		return Status;
	}

	BOOL Render(VOID)
	{
		BOOL Status = TRUE;
		
		if (pICommandAllocator->Reset() != S_OK)
		{
			Status = FALSE;
			Console::Write("Error: Failed to reset command allocator\n");
		}

		if (Status == TRUE)
		{
			if (pICommandList->Reset(pICommandAllocator, NULL) != S_OK)
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
			barrier.Transition.pResource = pIRenderBuffers[FrameIndex];
			barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

			pICommandList->ResourceBarrier(1, &barrier);
		}

		if (Status == TRUE)
		{
			D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = pIDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
			rtvHandle.ptr += FrameIndex * DescriptorIncrement;

			pICommandList->ClearRenderTargetView(rtvHandle, ClearColor, 0, NULL);
		}

		if (Status == TRUE)
		{
			D3D12_RESOURCE_BARRIER barrier = {};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.Transition.pResource = pIRenderBuffers[FrameIndex];
			barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

			pICommandList->ResourceBarrier(1, &barrier);
		}

		if (Status == TRUE)
		{
			if (pICommandList->Close() != S_OK)
			{
				Status = FALSE;
				Console::Write("Error: Could not finalize command list\n");
			}
		}

		if (Status == TRUE)
		{
			ID3D12CommandList* pICommandLists[] = { pICommandList };
			pICommandQueue->ExecuteCommandLists(_countof(pICommandLists), pICommandLists);
		}

		if (Status == TRUE)
		{
			if (pISwapChain->Present(1, 0) != S_OK)
			{
				Status = FALSE;
				Console::Write("Error: Failed to present\n");
			}
		}

		if (Status == TRUE)
		{
			if (pICommandQueue->Signal(pIFence, FenceValue) != S_OK)
			{
				Status = FALSE;
				Console::Write("Error: Failed to signal command queue fence\n");
			}
		}

		if (Status == TRUE)
		{
			if (pIFence->GetCompletedValue() < FenceValue)
			{
				if (pIFence->SetEventOnCompletion(FenceValue, hFenceEvent) == S_OK)
				{
					WaitForSingleObject(hFenceEvent, INFINITE);
				}
				else
				{
					Status = FALSE;
					Console::Write("Error: Failed to wait for completion fence\n");
				}
			}

			FenceValue++;
		}

		if (Status == TRUE)
		{
			FrameIndex = pISwapChain->GetCurrentBackBufferIndex();
		}

		return Status;
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

	if (Status == TRUE)
	{
		Status = Renderer::Initialize();
	}

	return Status;
}

VOID Uninitialize(VOID)
{
	Renderer::Uninitialize();

	Window::Uninitialize();

	Console::Uninitialize();

	Memory::Uninitialize();
}

BOOL Run(VOID)
{
	BOOL Status = TRUE;
	MSG msg = { 0 };

	while (Window::Open() && (Status == TRUE))
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) != FALSE)
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
			Status = Renderer::Render();
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

	return Status == TRUE ? 0 : -1;
}
