#include "IRenderer.hpp"
#include "CRenderer.hpp"

#include <windows.h>

#include <d3dcompiler.h>

#include <cmath>
#include <vector>

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
#if _DEBUG
	m_hDxgiDebugModule = NULL;
	m_pfnDxgiGetDebugInterface = NULL;
	m_pIDxgiDebugInterface = NULL;
#endif
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
	m_pIRootSignature = NULL;
	m_pIPipelineState = NULL;
	m_pIVertexBuffer = NULL;

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

	if (D3D12GetDebugInterface(__uuidof(ID3D12Debug), reinterpret_cast<VOID**>(&m_pID3D12DebugInterface)) == S_OK)
	{
		m_pID3D12DebugInterface->EnableDebugLayer();
	}
	else
	{
		Status = FALSE;
		Console::Write("Error: Failed to get dx12 debug interface\n");
	}

#if _DEBUG
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
		if (m_pfnDxgiGetDebugInterface(__uuidof(IDXGIDebug), reinterpret_cast<VOID**>(&m_pIDxgiDebugInterface)) != S_OK)
		{
			Status = FALSE;
			Console::Write("Error: Failed to get dxgi debug interface\n");
		}
	}
#endif

	if (Status == TRUE)
	{
		UINT Flags = 0;

#if _DEBUG
		Flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

		if (CreateDXGIFactory2(Flags, __uuidof(IDXGIFactory7), reinterpret_cast<VOID**>(&m_pIDxgiFactory)) != S_OK)
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
		D3D12CreateDevice(m_pIDxgiAdapter, D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), reinterpret_cast<VOID**>(&m_pIDevice));

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

		if (m_pIDevice->CreateCommandQueue(&cmdQueueDesc, __uuidof(ID3D12CommandQueue), reinterpret_cast<VOID**>(&m_pICommandQueue)) != S_OK)
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
			pISwapChain1->QueryInterface(__uuidof(IDXGISwapChain4), reinterpret_cast<VOID**>(&m_pISwapChain));
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

		if (m_pIDevice->CreateDescriptorHeap(&descHeap, __uuidof(ID3D12DescriptorHeap), reinterpret_cast<VOID**>(&m_pIDescriptorHeap)) == S_OK)
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
			if (m_pISwapChain->GetBuffer(i, __uuidof(ID3D12Resource), reinterpret_cast<VOID**>(&m_pIRenderBuffers[i])) != S_OK)
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
		if (m_pIDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), reinterpret_cast<VOID**>(&m_pICommandAllocator)) != S_OK)
		{
			Status = FALSE;
			Console::Write("Error: Could not create command allocator\n");
		}
	}

	if (Status == TRUE)
	{
		D3D12_ROOT_SIGNATURE_DESC desc = { };
		desc.NumParameters = 0;
		desc.pParameters = NULL;
		desc.NumStaticSamplers = 0;
		desc.pStaticSamplers = NULL;
		desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		ID3DBlob* pSignature = NULL;
		ID3DBlob* pError = NULL;

		if (D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &pSignature, &pError) == S_OK)
		{
			if (m_pIDevice->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), __uuidof(ID3D12RootSignature), reinterpret_cast<VOID**>(&m_pIRootSignature)) != S_OK)
			{
				Status = FALSE;
				Console::Write("Error: Could not create root signature\n");
			}
		}
		else
		{
			Status = FALSE;
			Console::Write("Error: Could not initialize root signature\n");

			if (pError != NULL)
			{
				Console::Write("Error Info: %s\n", pError->GetBufferPointer());
			}
		}

		if (pSignature != NULL)
		{
			pSignature->Release();
			pSignature = NULL;
		}

		if (pError != NULL)
		{
			pError->Release();
			pError = NULL;
		}
	}

	if (Status == TRUE)
	{
		Status = CompileShaders();
	}

	if (Status == TRUE)
	{
		if (m_pIDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_pICommandAllocator, m_pIPipelineState, __uuidof(ID3D12GraphicsCommandList), reinterpret_cast<VOID**>(&m_pICommandList)) != S_OK)
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

	if (Status == TRUE)
	{
		Status = CreateBuffers();
	}

	if (Status == TRUE)
	{
		m_Viewport.TopLeftX = 0;
		m_Viewport.TopLeftY = 0;
		m_Viewport.Width = static_cast<float>(Width);
		m_Viewport.Height = static_cast<float>(Height);
		m_Viewport.MinDepth = D3D12_MIN_DEPTH;
		m_Viewport.MaxDepth = D3D12_MAX_DEPTH;

		m_ScissorRect.left = 0;
		m_ScissorRect.top = 0;
		m_ScissorRect.right = Width;
		m_ScissorRect.bottom = Height;
	}

	return Status;
}

VOID CRenderer::Uninitialize(VOID)
{
	if (m_pIVertexBuffer != NULL)
	{
		m_pIVertexBuffer->Release();
		m_pIVertexBuffer = NULL;
	}

	if (m_pIUploadHeap != NULL)
	{
		m_pIUploadHeap->Release();
		m_pIUploadHeap = NULL;
	}

	if (m_pIPrimaryHeap != NULL)
	{
		m_pIPrimaryHeap->Release();
		m_pIPrimaryHeap = NULL;
	}

	if (m_pIPipelineState != NULL)
	{
		m_pIPipelineState->Release();
		m_pIPipelineState = NULL;
	}

	if (m_pIRootSignature != NULL)
	{
		m_pIRootSignature->Release();
		m_pIRootSignature = NULL;
	}

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

#if _DEBUG
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
#endif

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
		HRESULT result = m_pIDxgiFactory->EnumAdapterByGpuPreference(uIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, __uuidof(IDXGIAdapter4), reinterpret_cast<VOID**>(&pIAdapter));

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

BOOL CRenderer::CompileShader(LPCWSTR pFileName, LPCSTR pEntrypoint, LPCSTR pTarget, ID3DBlob** pShader)
{
	BOOL Status = TRUE;
	ID3DBlob* pError = NULL;
	UINT Flags = 0;

#if _DEBUG
	Flags |= D3DCOMPILE_DEBUG;
	Flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	if (D3DCompileFromFile(pFileName, NULL, NULL, pEntrypoint, pTarget, Flags, 0, pShader, &pError) != S_OK)
	{
		Status = FALSE;
		Console::Write("Error: Could not compile shader %s\n", pFileName);

		if (pError != NULL)
		{
			Console::Write("Error Info: %s\n", pError->GetBufferPointer());
		}
	}

	if (pError != NULL)
	{
		pError->Release();
		pError = NULL;
	}

	return Status;
}

BOOL CRenderer::CompileShaders(VOID)
{
	BOOL Status = TRUE;
	ID3DBlob* pVertexShader = NULL;
	ID3DBlob* pPixelShader = NULL;

	if (Status == TRUE)
	{
		if (CompileShader(L"C:/Workspace/DX12_HelloCube/Shaders/VertexShader.hlsl", "main", "vs_5_0", &pVertexShader) != TRUE)
		{
			Status = FALSE;
			Console::Write("Error: Failed to compile vertex shader\n");
		}
	}

	if (Status == TRUE)
	{
		if (CompileShader(L"C:/Workspace/DX12_HelloCube/Shaders/PixelShader.hlsl", "main", "ps_5_0", &pPixelShader) != TRUE)
		{
			Status = FALSE;
			Console::Write("Error: Failed to compile pixel shader\n");
		}
	}

	D3D12_INPUT_ELEMENT_DESC InputDescriptors[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	if (Status == TRUE)
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};

		desc.pRootSignature = m_pIRootSignature;

		desc.VS.pShaderBytecode = pVertexShader->GetBufferPointer();
		desc.VS.BytecodeLength = pVertexShader->GetBufferSize();
		desc.PS.pShaderBytecode = pPixelShader->GetBufferPointer();
		desc.PS.BytecodeLength = pPixelShader->GetBufferSize();
		desc.DS.pShaderBytecode = 0;
		desc.DS.BytecodeLength = 0;
		desc.HS.pShaderBytecode = 0;
		desc.HS.BytecodeLength = 0;
		desc.GS.pShaderBytecode = 0;
		desc.GS.BytecodeLength = 0;

		desc.StreamOutput.pSODeclaration = NULL;
		desc.StreamOutput.NumEntries = 0;
		desc.StreamOutput.pBufferStrides = NULL;
		desc.StreamOutput.NumStrides = 0;
		desc.StreamOutput.RasterizedStream = 0;

		desc.BlendState.AlphaToCoverageEnable = FALSE;
		desc.BlendState.IndependentBlendEnable = FALSE;
		for (unsigned int i = 0; i < 8; i++)
		{
			desc.BlendState.RenderTarget[i].BlendEnable = FALSE;
			desc.BlendState.RenderTarget[i].LogicOpEnable = FALSE;
			desc.BlendState.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
			desc.BlendState.RenderTarget[i].DestBlend = D3D12_BLEND_ZERO;
			desc.BlendState.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
			desc.BlendState.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
			desc.BlendState.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
			desc.BlendState.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
			desc.BlendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;
			desc.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		}

		desc.SampleMask = UINT_MAX;

		desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
		desc.RasterizerState.FrontCounterClockwise = FALSE;
		desc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		desc.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		desc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		desc.RasterizerState.DepthClipEnable = TRUE;
		desc.RasterizerState.MultisampleEnable = FALSE;
		desc.RasterizerState.AntialiasedLineEnable = FALSE;
		desc.RasterizerState.ForcedSampleCount = 0;
		desc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

		desc.DepthStencilState.DepthEnable = FALSE;
		desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		desc.DepthStencilState.DepthFunc = static_cast<D3D12_COMPARISON_FUNC>(0);
		desc.DepthStencilState.StencilEnable = FALSE;
		desc.DepthStencilState.StencilReadMask = 0;
		desc.DepthStencilState.StencilWriteMask = 0;
		desc.DepthStencilState.FrontFace.StencilFailOp = static_cast<D3D12_STENCIL_OP>(0);
		desc.DepthStencilState.FrontFace.StencilDepthFailOp = static_cast<D3D12_STENCIL_OP>(0);
		desc.DepthStencilState.FrontFace.StencilPassOp = static_cast<D3D12_STENCIL_OP>(0);
		desc.DepthStencilState.FrontFace.StencilFunc = static_cast<D3D12_COMPARISON_FUNC>(0);
		desc.DepthStencilState.BackFace.StencilFailOp = static_cast<D3D12_STENCIL_OP>(0);
		desc.DepthStencilState.BackFace.StencilDepthFailOp = static_cast<D3D12_STENCIL_OP>(0);
		desc.DepthStencilState.BackFace.StencilPassOp = static_cast<D3D12_STENCIL_OP>(0);
		desc.DepthStencilState.BackFace.StencilFunc = static_cast<D3D12_COMPARISON_FUNC>(0);

		desc.InputLayout.pInputElementDescs = InputDescriptors;
		desc.InputLayout.NumElements = _countof(InputDescriptors);

		desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		
		desc.NumRenderTargets = 1;
		desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.DSVFormat = DXGI_FORMAT_UNKNOWN;

		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;

		desc.NodeMask = 0;

		desc.CachedPSO.pCachedBlob = NULL;
		desc.CachedPSO.CachedBlobSizeInBytes = 0;

		desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

		if (m_pIDevice->CreateGraphicsPipelineState(&desc, __uuidof(ID3D12PipelineState), reinterpret_cast<void**>(&m_pIPipelineState)) != S_OK)
		{
			Status = false;
			Console::Write("Error: Failed to create graphics pipeline state object\n");
		}
	}

	if (pVertexShader != NULL)
	{
		pVertexShader->Release();
		pVertexShader = NULL;
	}

	if (pPixelShader != NULL)
	{
		pPixelShader->Release();
		pPixelShader = NULL;
	}

	return Status;
}

BOOL CRenderer::CreateBuffers(VOID)
{
	BOOL Status = TRUE;

	/*
	* 
	*		   5 _______________ 6
	*		    /| 			   /|
	*		   / |			  / |
	*		  /  |			 /  |
	*	   1 /___|__________/ 2 |
	*		 |	 |4 	   |	|
	*		 |   |_________|____| 7
	*		 |	 /		   |   /
	*		 |	/		   |  /
	*		 | /		   | /
	*	   0 |/____________|/ 3
	* 
	* 0: { -0.5, -0.5, +0.5 }
	* 1: { -0.5, +0.5, +0.5 }
	* 2: { +0.5, +0.5, +0.5 }
	* 3: { +0.5, -0.5, +0.5 }
	* 4: { -0.5, -0.5, -0.5 }
	* 5: { -0.5, +0.5, -0.5 }
	* 6: { +0.5, +0.5, -0.5 }
	* 7: { +0.5, -0.5, -0.5 }
	*/

	const float Vertices[][3] =
	{
		{ -0.5, -0.5, +0.5 },
		{ -0.5, +0.5, +0.5 },
		{ +0.5, +0.5, +0.5 },
		{ +0.5, -0.5, +0.5 },
		{ -0.5, -0.5, -0.5 },
		{ -0.5, +0.5, -0.5 },
		{ +0.5, +0.5, -0.5 },
		{ +0.5, -0.5, -0.5 }
	};

	struct Triangle
	{
		unsigned short Indices[6];
		float Normal[3];
		float Colour[3];
	};

	const Triangle Triangles[] =
	{
		// front
		{
			{
				0, 1, 2,
				0, 2, 3
			},
			{ 0.0f, 0.0f, +1.0f },
			{ 1.0f, 0.0f,  0.0f }
		},

		// back
		{
			{
				4, 5, 6,
				4, 6, 7
			},
			{ 0.0f, 0.0f, -1.0f },
			{ 0.0f, 1.0f,  0.0f }
		},

		// left
		{
			{
				0, 1, 5,
				0, 5, 4
			},
			{ -1.0f, 0.0f, 0.0f },
			{  0.0f, 0.0f, 1.0f }
		},

		// right
		{
			{
				3, 2, 6,
				3, 6, 7
			},
			{ +1.0f, 0.0f, 0.0f },
			{  0.5f, 0.0f, 1.0f }
		},

		// top
		{
			{
				1, 2, 5,
				2, 5, 6
			},
			{ 0.0f, +1.0f, 0.0f },
			{ 1.0f,  1.0f, 0.0f }
		},

		// bottom
		{
			{
				0, 3, 4,
				3, 4, 7
			},
			{ 0.0f, -1.0f, 0.0f },
			{ 1.0f,  0.0f, 1.0f }
		}
	};

	std::vector<float> VertexArray;
	for (unsigned int i = 0; i < _countof(Triangles); i++)
	{
		const Triangle& t = Triangles[i];
		for (unsigned int j = 0; j < 6; j++)
		{
			VertexArray.insert(VertexArray.end(), Vertices[t.Indices[j]], Vertices[t.Indices[j]] + 3);
			VertexArray.insert(VertexArray.end(), t.Colour, t.Colour + 3);
		}
	}

	// DEBUG:
	VertexArray.clear();
	VertexArray =
	{
		// top
		0.0f, 0.25f, 0.0f, // position
		1.0f, 0.00f, 0.0f, // color

		// right
		0.25f, 0.0f, 0.0f, // position
		0.0f, 1.0f, 0.0f,

		// left
		-0.25f, 0.0f, 0.0f, // position
		0.0f, 0.0f, 1.0f, // color
	};

	const uint32_t UPLOAD_HEAP_SIZE = 64 * 1024 * 1024; // 64 KB
	const uint32_t PRIMARY_HEAP_SIZE = 64 * 1024 * 1024; // 64 KB

	if (Status == TRUE)
	{
		D3D12_RESOURCE_DESC uploadResourceDesc = { };
		uploadResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		uploadResourceDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		uploadResourceDesc.Width = UPLOAD_HEAP_SIZE;
		uploadResourceDesc.Height = 1;
		uploadResourceDesc.DepthOrArraySize = 1;
		uploadResourceDesc.MipLevels = 1;
		uploadResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		uploadResourceDesc.SampleDesc.Count = 1;
		uploadResourceDesc.SampleDesc.Quality = 0;
		uploadResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		uploadResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		D3D12_RESOURCE_ALLOCATION_INFO uploadAllocationInfo = m_pIDevice->GetResourceAllocationInfo(0, 1, &uploadResourceDesc);

		D3D12_HEAP_DESC uploadHeapDesc = { };
		uploadHeapDesc.SizeInBytes = uploadAllocationInfo.SizeInBytes;
		uploadHeapDesc.Properties.Type = D3D12_HEAP_TYPE_UPLOAD;
		uploadHeapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		uploadHeapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		uploadHeapDesc.Properties.CreationNodeMask = 1;
		uploadHeapDesc.Properties.VisibleNodeMask = 1;

		if (m_pIDevice->CreateHeap(&uploadHeapDesc, __uuidof(ID3D12Heap), reinterpret_cast<void**>(&m_pIUploadHeap)) != S_OK)
		{
			Status = FALSE;
			Console::Write("Error: Failed to create upload heap\n");
		}
	}

	if (Status == TRUE)
	{
		D3D12_RESOURCE_DESC primaryResourceDesc = { };
		primaryResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		primaryResourceDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		primaryResourceDesc.Width = PRIMARY_HEAP_SIZE;
		primaryResourceDesc.Height = 1;
		primaryResourceDesc.DepthOrArraySize = 1;
		primaryResourceDesc.MipLevels = 1;
		primaryResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		primaryResourceDesc.SampleDesc.Count = 1;
		primaryResourceDesc.SampleDesc.Quality = 0;
		primaryResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		primaryResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		D3D12_RESOURCE_ALLOCATION_INFO primaryAllocationInfo = m_pIDevice->GetResourceAllocationInfo(0, 1, &primaryResourceDesc);

		D3D12_HEAP_PROPERTIES primaryHeapProps = {};
		primaryHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
		primaryHeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		primaryHeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		primaryHeapProps.CreationNodeMask = 1;
		primaryHeapProps.VisibleNodeMask = 1;

		D3D12_HEAP_DESC primaryHeapDesc = { };
		primaryHeapDesc.SizeInBytes = primaryAllocationInfo.SizeInBytes;
		primaryHeapDesc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
		primaryHeapDesc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		primaryHeapDesc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		primaryHeapDesc.Properties.CreationNodeMask = 1;
		primaryHeapDesc.Properties.VisibleNodeMask = 1;

		if (m_pIDevice->CreateHeap(&primaryHeapDesc, __uuidof(ID3D12Heap), reinterpret_cast<void**>(&m_pIPrimaryHeap)) != S_OK)
		{
			Status = FALSE;
			Console::Write("Error: Failed to create primary heap\n");
		}
	}

	ID3D12Resource* vertexDataUploadBuffer = NULL;

	if (Status == TRUE)
	{
		D3D12_RESOURCE_DESC vertexBufferDesc = {};
		vertexBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		vertexBufferDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		vertexBufferDesc.Width = sizeof(float) * VertexArray.size();
		vertexBufferDesc.Height = 1;
		vertexBufferDesc.DepthOrArraySize = 1;
		vertexBufferDesc.MipLevels = 1;
		vertexBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
		vertexBufferDesc.SampleDesc.Count = 1;
		vertexBufferDesc.SampleDesc.Quality = 0;
		vertexBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		vertexBufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		if (m_pIDevice->CreatePlacedResource(m_pIUploadHeap, 0, &vertexBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, __uuidof(ID3D12Resource), reinterpret_cast<void**>(&vertexDataUploadBuffer)) != S_OK)
		{
			Status = FALSE;
			Console::Write("Error: Failed to create vertex buffer upload allocation\n");
		}

		if (m_pIDevice->CreatePlacedResource(m_pIPrimaryHeap, 0, &vertexBufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, NULL, __uuidof(ID3D12Resource), reinterpret_cast<void**>(&m_pIVertexBuffer)) != S_OK)
		{
			Status = FALSE;
			Console::Write("Error: Failed to create vertex buffer primary allocation\n");
		}
	}

	// Copy the vertex data to the upload heap
	if (Status == TRUE)
	{
		BYTE* pVertexData = NULL;

		D3D12_RANGE range = {};
		range.Begin = 0;
		range.End = 0;

		if (vertexDataUploadBuffer->Map(0, &range, reinterpret_cast<void**>(&pVertexData)) != S_OK)
		{
			Status = FALSE;
			Console::Write("Error: Failed to map vertex buffer allocation\n");
		}

		if (Status == TRUE)
		{
			CopyMemory(pVertexData, VertexArray.data(), sizeof(float)* VertexArray.size());
			vertexDataUploadBuffer->Unmap(0, NULL);
		}
	}

	// Copy the vertex data from the upload heap to the primary heap
	if (Status == TRUE)
	{
		m_pICommandList->CopyBufferRegion(m_pIVertexBuffer, 0, vertexDataUploadBuffer, 0, sizeof(float)* VertexArray.size());

		if (Status == TRUE)
		{
			D3D12_RESOURCE_BARRIER barrier = {};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.Transition.pResource = m_pIVertexBuffer;
			barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

			m_pICommandList->ResourceBarrier(1, &barrier);
		}

		if (m_pICommandList->Close() != S_OK)
		{
			Status = FALSE;
			Console::Write("Error: Could not finalize command list\n");
		}

		ID3D12CommandList* pICommandLists[] = { m_pICommandList };
		m_pICommandQueue->ExecuteCommandLists(_countof(pICommandLists), pICommandLists);

		m_VertexBufferView.BufferLocation = m_pIVertexBuffer->GetGPUVirtualAddress();
		m_VertexBufferView.SizeInBytes = sizeof(float) * VertexArray.size();
		m_VertexBufferView.StrideInBytes = sizeof(float) * 6;
	}

	if (Status == TRUE)
	{
		Status = WaitForFrame();
	}

	if (vertexDataUploadBuffer != NULL)
	{
		vertexDataUploadBuffer->Release();
	}

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
		if (m_pICommandList->Reset(m_pICommandAllocator, m_pIPipelineState) != S_OK)
		{
			Status = FALSE;
			Console::Write("Error: Failed to reset command list\n");
		}
	}

	if (Status == TRUE)
	{
		m_pICommandList->SetGraphicsRootSignature(m_pIRootSignature);
		m_pICommandList->RSSetViewports(1, &m_Viewport);
		m_pICommandList->RSSetScissorRects(1, &m_ScissorRect);
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

		m_pICommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

		// Clear the screen
		m_pICommandList->ClearRenderTargetView(rtvHandle, ClearColor, 0, NULL);

		// Draw the cube
		m_pICommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_pICommandList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
		//m_pICommandList->DrawInstanced(36, 1, 0, 0);
		m_pICommandList->DrawInstanced(3, 1, 0, 0);
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
		Status = WaitForFrame();
	}

	return Status;
}

BOOL CRenderer::WaitForFrame(VOID)
{
	BOOL Status = TRUE;

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
