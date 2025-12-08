#include "base/core.h"
#include "os_inc.h"

#if OS_WINDOWS
#include "win32/os_core_win32.cpp"
#elif OS_LINUX
#include "linux/os_core_linux.cpp"
#endif