#ifndef CMEMORY_HPP
#define CMEMORY_HPP

#include "Defines.hpp"

class CMemory
{
protected:
	HANDLE m_hHeap;

public:
	CMemory();
	~CMemory();

	BOOL Initialize(VOID);
	VOID Uninitialize(VOID);

	PVOID Allocate(SIZE_T nBytes, BOOL bClear);
	BOOL Free(PVOID pMemory);
};

#endif // CMEMORY_HPP