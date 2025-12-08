#pragma once

#include "base/core.h"

#if OS_WINDOWS
#include "os/win32/os_core_win32.h"
#elif OS_LINUX
#include "os/linux/os_core_linux.h"
#endif