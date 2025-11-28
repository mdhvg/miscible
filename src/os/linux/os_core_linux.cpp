#include "os/linux/os_core_linux.h"

#if defined(OS_LINUX)
#include <sys/stat.h>

Guid os_make_guid()
{
	Guid guid = {0};
	getrandom(guid.v, sizeof(guid.v), 0);
	guid.data3 &= 0x0fff;
	guid.data3 |= (4 << 12);
	guid.data4[0] &= 0x3f;
	guid.data4[0] |= 0x80;
	return guid;
}

void os_prelaunch()
{
	struct stat st = {};
	if (stat(ATLAS_DIR, &st) == -1)
	{
		ASSERT(mkdir(ATLAS_DIR, 0755) != -1);
	}
}

#endif