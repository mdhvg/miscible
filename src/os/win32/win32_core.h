#pragma once
#define NOMINMAX
#include <Windows.h>

#define OS_THREAD_ROUTINE(name) DWORD name(LPVOID data)
#define OS_THREAD_ROUTINE_T     LPTHREAD_START_ROUTINE

#define OSChar     wchar
#define OSSlash    "\\"
#define W(x)       Glue(L, x)
#define Semaphore  HANDLE
#define Mutex      CRITICAL_SECTION
#define Thread     HANDLE
#define LibHandle  HMODULE
#define LibAddress FARPROC
#define LibExt     ".dll"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "Rpcrt4.lib")
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Comdlg32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "normaliz.lib")
#pragma comment(lib, "wldap32.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "secur32.lib")

typedef HANDLE FileHandle;

struct OSMmap
{
    void *data;
    U64 size;
    HANDLE handle;
};

typedef struct OSInfo OSInfo;
struct OSInfo
{
    OS_COMMON;
    U64 microsecond_resolution;
};

inline void win32_sleep_ms(U64 ms)
{
    Sleep(ms);
}

MSCBL_API void win32_format_path(StringBuilder *dir);
void win32_to_unix_timestamp(U64 *win_timestamp);

inline LibAddress os_libfunc(LibHandle lib, const char *symbol)
{
    return GetProcAddress(lib, symbol);
}
inline void os_closelib(LibHandle lib)
{
    FreeLibrary(lib);
}

Thread os_thread_launch(OS_THREAD_ROUTINE_T fn, struct Worker *worker);
