#pragma once

#include "base/base_core.h"

#if OS_WINDOWS
#include "os/win32/os_core_win32.h"
#elif OS_LINUX
#include "os/linux/os_core_linux.h"
#endif

global OSInfo os_info = {0};