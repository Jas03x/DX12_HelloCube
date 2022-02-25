
#include <Strsafe.h>
#include <Windows.h>

namespace Memory
{
	HANDLE hHeap;

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

	return Status;
}

VOID Uninitialize(VOID)
{
	Console::Uninitialize();

	Memory::Uninitialize();
}

INT main(INT ArgC, CHAR* ArgV[])
{
	BOOL Status = TRUE;

	Status = Initialize();

	Console::Write("Initializing\n");

	Console::Write("Test %u %u %u\n", 1, 2, 3);

	// WNDCLASSEX 

	//RegisterClassEx();

	Uninitialize();

	return 0;
}
