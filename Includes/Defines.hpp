#ifndef DEFINES_HPP
#define DEFINES_HPP

#define CONST	const
#define VOID	void

#ifndef NULL
	#define NULL 0
#endif

#define TRUE	1
#define FALSE   0

typedef signed char			int8_t;
typedef short				int16_t;
typedef int					int32_t;
typedef long long			int64_t;
typedef unsigned char		uint8_t;
typedef unsigned short		uint16_t;
typedef unsigned int		uint32_t;
typedef unsigned long long	uint64_t;

typedef int					BOOL;
typedef char				CHAR;
typedef short				SHORT;
typedef int					INT;
typedef unsigned int        UINT;
typedef long				LONG;
typedef unsigned long		ULONG;
typedef unsigned __int64	SIZE_T;

typedef unsigned __int64    UINT64;

typedef unsigned long		DWORD;

typedef float				FLOAT;

typedef char*				PCHAR;
typedef const char*			LPCCH;
typedef const char*			LPCSTR;
typedef const wchar_t*		LPCWSTR;

typedef unsigned __int64    WPARAM;
typedef __int64				LPARAM;

typedef long				HRESULT;
typedef __int64             LRESULT;

typedef unsigned short      ATOM;
typedef void*				HANDLE;
typedef struct HWND__*		HWND;
typedef struct HINSTANCE__* HMODULE;
typedef struct HINSTANCE__* HINSTANCE;
typedef void*				PVOID;

typedef char*				va_list;

#endif // DEFINES_HPP