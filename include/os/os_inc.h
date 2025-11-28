#include "base/core.h"

#if defined(OS_LINUX)
#include "os/linux/os_core_linux.h"
#elif defined(OS_WINDOWS)
#include "os/win32/os_core_win32.h"
#endif