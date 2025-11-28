#include "base/core.h"

#if defined(OS_LINUX)

#include <sys/random.h>

Guid os_make_guid();

void os_prelaunch();

#endif