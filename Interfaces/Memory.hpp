#ifndef MEMORY_HPP
#define MEMORY_HPP

#include "Defines.hpp"

class Memory
{
public:
	static BOOL Initialize(VOID);
	static VOID Uninitialize(VOID);

	static PVOID Allocate(SIZE_T nBytes, BOOL bClear);
	static BOOL Free(PVOID pMemory);
};

#endif // MEMORY_HPP