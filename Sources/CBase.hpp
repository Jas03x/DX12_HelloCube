#ifndef CBASE_HPP
#define CBASE_HPP

#include "Defines.hpp"

class CBase
{
public:
	CBase();
	~CBase();

	PVOID operator new(SIZE_T size);
	VOID  operator delete(PVOID ptr);
};

#endif // CBASE_HPP