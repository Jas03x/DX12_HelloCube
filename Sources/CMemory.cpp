#include "Memory.hpp"
#include "CMemory.hpp"

#include <windows.h>

CMemory g_Memory;

BOOL Memory::Initialize(VOID)
{
	return g_Memory.Initialize();
}

VOID Memory::Uninitialize(VOID)
{
	g_Memory.Uninitialize();
}

PVOID Memory::Allocate(SIZE_T nBytes, BOOL bClear)
{
	return g_Memory.Allocate(nBytes, bClear);
}

BOOL Memory::Free(PVOID pMemory)
{
	return g_Memory.Free(pMemory);
}

CMemory::CMemory()
{
	m_hHeap = NULL;
}

CMemory::~CMemory()
{
}

BOOL CMemory::Initialize(VOID)
{
	BOOL Status = TRUE;

	m_hHeap = GetProcessHeap();

	if (m_hHeap == NULL)
	{
		Status = FALSE;
	}

	return Status;
}

VOID CMemory::Uninitialize(VOID)
{
	m_hHeap = NULL;
}

PVOID CMemory::Allocate(SIZE_T nBytes, BOOL bClear)
{
	DWORD Flags = 0;

	if (bClear)
	{
		Flags |= HEAP_ZERO_MEMORY;
	}

	return HeapAlloc(m_hHeap, Flags, nBytes);
}

BOOL CMemory::Free(PVOID pMemory)
{
	BOOL Status = TRUE;

	Status = HeapFree(m_hHeap, 0, pMemory);

	return Status;
}
