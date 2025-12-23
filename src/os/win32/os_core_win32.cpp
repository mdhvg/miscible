#include <handleapi.h>
#include <memoryapi.h>
#include <minwindef.h>
#include <processthreadsapi.h>
#include <synchapi.h>
#include <sysinfoapi.h>
#include <winbase.h>
#include <winnt.h>

#include "os_core_win32.h"
#include "base/base_core.h"
#include "base/threadpool.h"

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
		Assert(CreateDirectoryA(ATLAS_DIR, NULL) || GetLastError() == ERROR_ALREADY_EXISTS);
	}
	{
		os_info.microsecond_resolution = 1;
		LARGE_INTEGER large_int_resolution;
		if (QueryPerformanceFrequency(&large_int_resolution))
		{
			os_info.microsecond_resolution = large_int_resolution.QuadPart;
		}
	}

	SYSTEM_INFO sysinfo = {0};
	GetSystemInfo(&sysinfo);
	os_info.page_size = sysinfo.dwPageSize;
}

internal U64 os_now_microseconds(void)
{
	U64 result = 0;
	LARGE_INTEGER large_int_counter;
	if (QueryPerformanceCounter(&large_int_counter))
	{
		result = (large_int_counter.QuadPart * Mil(1)) / os_info.microsecond_resolution;
	}
	return result;
}

internal U32 win32_sleep_ms_from_us(U64 end_us)
{
	if (end_us == U64_MAX) return INFINITE;

	U32 sleep_ms = 0;
	U64 begint	 = os_now_microseconds();
	if (begint < end_us)
	{
		U64 sleep_us = end_us - begint;
		sleep_ms	 = (U32)((sleep_us + 999) / 1000);
	}
	return sleep_ms;
}

void *os_reserve(void *ptr, U64 size)
{
	void *result = VirtualAlloc(ptr, size, MEM_RESERVE, PAGE_READWRITE);

	if (!result)
	{
		U32 err = GetLastError();
		msc_log_error(OS_WIN32, "Error Code: 0x%X (%lu)\n", err, err);
		Assert(0);
	}
	return result;
}

B32 os_release(void *ptr)
{
	B32 result = (VirtualFree(ptr, 0, MEM_RELEASE) != 0);

	if (!result)
	{
		U32 err = GetLastError();
		msc_log_error(OS_WIN32, "Error Code: 0x%X (%lu)\n", err, err);
		Assert(0);
	}
	return result;
}

B32 os_commit(void *ptr, U64 size)
{
	B32 result = (VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE) != 0);

	if (!result)
	{
		U32 err = GetLastError();
		msc_log_error(OS_WIN32, "Error Code: 0x%X (%lu)\n", err, err);
		Assert(0);
	}
	return result;
}

B32 os_decommit(void *ptr, U64 size)
{
	B32 result = (VirtualFree(ptr, size, MEM_DECOMMIT) != 0);

	if (!result)
	{
		U32 err = GetLastError();
		msc_log_error(OS_WIN32, "Error Code: 0x%X (%lu)\n", err, err);
		Assert(0);
	}
	return result;
}

Semaphore os_semaphore_alloc(U32 initial, U32 max)
{
	HANDLE handle = CreateSemaphore(0, initial, max, NULL);
	return {(U64)handle};
}

void os_semaphore_release(Semaphore s)
{
	HANDLE h = (HANDLE)s.u64[0];
	CloseHandle(h);
}

void os_semaphore_drop(Semaphore s)
{
	HANDLE handle = (HANDLE)s.u64[0];
	ReleaseSemaphore(handle, 1, 0);
}

B8 os_semaphore_take(Semaphore s, U64 end_us)
{
	U32 sleep		  = win32_sleep_ms_from_us(end_us);
	HANDLE h		  = (HANDLE)s.u64[0];
	DWORD wait_result = WaitForSingleObject(h, sleep);
	B8 result		  = (wait_result == WAIT_OBJECT_0);
	return result;
}

Thread os_thread_launch(LPTHREAD_START_ROUTINE fn, Worker *worker)
{
	HANDLE handle = CreateThread(0, 0, fn, worker, 0, NULL);
	return {IntFromPtr(handle)};
}

void os_thread_detach(Thread t)
{
	HANDLE h = (HANDLE)t.u64[0];
	if (h != 0)
	{
		CloseHandle(h);
	}
}
