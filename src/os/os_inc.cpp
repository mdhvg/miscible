#include "base/base_core.h"
#include "os/os_inc.h"

#if OS_WINDOWS
#include "os/win32/os_core_win32.cpp"
#elif OS_LINUX
#include "os/linux/os_core_linux.cpp"
#endif

OSInfo os_info = {0};
