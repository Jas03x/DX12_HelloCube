#include "CBase.hpp"

#include "Memory.hpp"

CBase::CBase()
{

}

CBase::~CBase()
{

}

PVOID CBase::operator new(SIZE_T size)
{
	return Memory::Allocate(size, TRUE);
}

VOID CBase::operator delete(PVOID ptr)
{
	Memory::Free(ptr);
}
