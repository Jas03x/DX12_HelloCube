#ifndef CONSOLE_HPP
#define CONSOLE_HPP

#include "Defines.hpp"

class Console
{
public:
	static BOOL Initialize(VOID);
	static VOID Uninitialize(VOID);

	static BOOL Write(LPCCH Msg, ...);
};

#endif // CONSOLE_HPP