#ifndef BASE_CORE_H
#define BASE_CORE_H

// Reference: https://github.com/EpicGamesExt/raddebugger/blob/master/src/base/base_core.h

#include <chrono>
#include <filesystem>
#include <wchar.h>

#include "nlohmann/json.hpp"
#include "spdlog/spdlog.h"

#define internal static
#define global	 static
#define local		 static

#if defined(__gnu_linux__) || defined(__linux__)
#define OS_LINUX
#elif defined(_WIN32)
#define OS_WINDOWS
#endif

#define MAX(A, B) (((A) > (B)) ? (A) : (B))
#define MIN(A, B) (((A) < (B)) ? (A) : (B))

#define KB(A) ((A) << 10)
#define MB(A) ((A) << 20)
#define GB(A) ((A) << 30)

#define Kil(A) ((A) * 1000)
#define Mil(A) ((A) * 1000000)
#define Bil(A) ((A) * 1000000000)

#define AlignOf(A) (A - 1) & ~(A - 1)

#define ToCeilInt(A, B) (((A) + (B - 1)) / (B))

#define BitSet(bitset, idx)		(bitset[(idx) / 64] |= (1ull << ((idx) % 64)))
#define BitClear(bitset, idx) (bitset[(idx) / 64] &= ~(1ull << ((idx) % 64)))
#define BitTest(bitset, idx)	(bitset[(idx) / 64] & (1ull << ((idx) % 64)))

#define MemoryCopy(dst, src, size) memmove((dst), (src), (size))
#define MemoryCopyArray(d, s)			 MemoryCopy((d), (s), sizeof(d))

#define MemoryZero(s, z)		memset((s), 0, (z))
#define MemoryZeroStruct(s) MemoryZero((s), sizeof(*(s)))

#if WCHAR_MAX > 0xffffu
#define WCHAR_UTF32
int sz = 32;
#else
#define WCHAR_UTF16
int sz = 16;
#endif

#define PERF(A, ...)                                                       \
	static auto start_##A = std::chrono::high_resolution_clock::now();       \
	__VA_ARGS__;                                                             \
	static auto end_##A = std::chrono::high_resolution_clock::now();         \
	static std::chrono::duration<double> duration_##A = end_##A - start_##A; \
	SPDLOG_INFO("PERF({}): {:.6f} seconds", #A, duration_##A.count())

#if defined(_WIN32)
#define TRAP() __debugbreak();
#else
#define TRAP() __builtin_trap();
#endif

#define ASSERT(x)                             \
	if (!(x))                                   \
	{                                           \
		SPDLOG_ERROR("Assertion failed: {}", #x); \
		TRAP();                                   \
	}

typedef uint8_t U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;
typedef int8_t S8;
typedef int16_t S16;
typedef int32_t S32;
typedef int64_t S64;
typedef S8 B8;
typedef S16 B16;
typedef S32 B32;
typedef S64 B64;
typedef wchar_t wchar;
typedef float F32;
typedef double F64;

using json = nlohmann::json;
namespace fs = std::filesystem;

union Guid {
	struct
	{
		U32 data1;
		U16 data2;
		U16 data3;
		U8 data4[8];
	};
	U8 v[16];
};

extern std::atomic<bool> running;

void bytes_as_hex_lower(U8 *data, U64 start, U64 len, char *out);
void bytes_as_hex_upper(U8 *data, U64 start, U64 len, char *out);

// TODO: Move these to some other place
#define ATLAS_DIR ROOT_DIR "/.atlas"
#define DB_PATH		ROOT_DIR "/pics.sqlite"

#endif // BASE_CORE_H