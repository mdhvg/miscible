#include "base/base_core.h"
#include "os/os_inc.h"

OSInfo os_info = {0};

#if OS_WIN32
#include "os/win32/win32_core.cpp"
#elif OS_LINUX
#include "os/linux/linux_core.cpp"
#endif
