#ifndef CRENDERER_HPP
#define CRENDERER_HPP

#include <d3d12.h>

#include "Defines.hpp"
#include "IRenderer.hpp"

typedef const struct _GUID& RGUID;

class CRenderer : public IRenderer
{
protected:
	enum								{ NumBuffers = 2 };

	static CONST FLOAT					ClearColor[];

	HWND								m_hWND;

#if _DEBUG
	HMODULE								m_hDxgiDebugModule;
	HRESULT								(*m_pfnDxgiGetDebugInterface)(RGUID, VOID**);

	class IDXGIDebug*					m_pIDxgiDebugInterface;
#endif

	class IDXGIFactory7*				m_pIDxgiFactory;
	class IDXGIAdapter4*				m_pIDxgiAdapter;
	class IDXGISwapChain4*				m_pISwapChain;

	class ID3D12Debug*					m_pID3D12DebugInterface;
	class ID3D12Device*					m_pIDevice;
	class ID3D12CommandQueue*			m_pICommandQueue;
	class ID3D12DescriptorHeap*			m_pIDescriptorHeap;
	class ID3D12Resource*				m_pIRenderBuffers[NumBuffers];
	class ID3D12CommandAllocator*		m_pICommandAllocator;
	class ID3D12GraphicsCommandList*	m_pICommandList;
	class ID3D12Fence*					m_pIFence;
	class ID3D12RootSignature*			m_pIRootSignature;
	class ID3D12PipelineState*			m_pIPipelineState;
	class ID3D12Resource*				m_pIVertexBuffer;

	D3D12_RECT							m_ScissorRect;
	D3D12_VIEWPORT						m_Viewport;
	D3D12_VERTEX_BUFFER_VIEW			m_VertexBufferView;

	HANDLE								m_hFenceEvent;

	UINT								m_FrameIndex;
	UINT64								m_FenceValue;
	SIZE_T								m_DescriptorIncrement;

protected:
	CRenderer();
	~CRenderer();

	BOOL Initialize(HWND hWND, ULONG Width, ULONG Height);
	VOID Uninitialize(VOID);

	BOOL PrintAdapterDesc(UINT uIndex, IDXGIAdapter4* pIAdapter);
	BOOL EnumerateDxgiAdapters(VOID);

	BOOL WaitForFrame(VOID);

	BOOL CompileShaders(VOID);
	BOOL CompileShader(LPCWSTR pFileName, LPCSTR pEntrypoint, LPCSTR pTarget, ID3DBlob** pShader);

	BOOL CreateBuffers(VOID);

public:
	static CRenderer* Create(HWND hWND, ULONG Width, ULONG Height);
	static VOID		  Destroy(CRenderer* pRenderer);

public:
	virtual BOOL Render(VOID);
};

#endif // CRENDERER_HPP