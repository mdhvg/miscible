#include "base/core.h"

#if defined(OS_WINDOWS)

#include <windows.h>

Guid os_make_guid();
void os_prelaunch();

#endif