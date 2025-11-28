#include "os/win32/os_core_win32.h"

#if defined(OS_WINDOWS)

Guid os_make_guid()
{
	Guid result;
	MemoryZeroStruct(&result);
	UUID uuid;
	RPC_STATUS rpc_status = UuidCreate(&uuid);
	if (rpc_status == RPC_S_OK)
	{
		result.data1 = uuid.Data1;
		result.data2 = uuid.Data2;
		result.data3 = uuid.Data3;
		MemoryCopyArray(result.data4, uuid.Data4);
	}
	return result;
}

void os_prelaunch()
{
	DWORD dwAttrib = GetFileAttributesA(ATLAS_DIR);
	if (dwAttrib == INVALID_FILE_ATTRIBUTES)
	{
		ASSERT(CreateDirectoryA(ATLAS_DIR, NULL) || GetLastError() == ERROR_ALREADY_EXISTS);
	}
}

#endif