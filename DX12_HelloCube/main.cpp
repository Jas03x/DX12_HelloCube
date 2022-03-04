
#include "Defines.hpp"

#include "DX12_HelloCube.hpp"

INT main(INT ArgC, CHAR* ArgV[])
{
	BOOL Status = DX12_HelloCube::Run();

	return Status == TRUE ? 0 : -1;
}
