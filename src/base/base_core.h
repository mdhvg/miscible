#pragma once
// A lot of it comes from: https://github.com/EpicGamesExt/raddebugger

#include <stdint.h>
#include <wchar.h>

#define global_v static
#define local_v  static

#if defined(_WIN32)
#define OS_WINDOWS 1
#elif defined(__gnu_linux__) || defined(__linux__)
#define OS_LINUX 1
#endif

#if defined(_MSC_VER)
#define COMPILER_MSVC 1
#if defined(_M_AMD64)
#define ARCH_X64 1
#elif defined(_M_IX86)
#define ARCH_X86 1
#elif defined(_M_ARM64)
#define ARCH_ARM64 1
#elif defined(_M_ARM)
#define ARCH_ARM32 1
#else
#error Architecture not supported.
#endif
#elif defined(__clang__)
#define COMPILER_CLANG 1
#if defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64)
#define ARCH_X64 1
#elif defined(i386) || defined(__i386) || defined(__i386__)
#define ARCH_X86 1
#elif defined(__aarch64__)
#define ARCH_ARM64 1
#elif defined(__arm__)
#define ARCH_ARM32 1
#else
#error Architecture not supported.
#endif
#elif defined(__GNUC__) || defined(__GNUG__)
#define COMPILER_GCC 1
#if defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64)
#define ARCH_X64 1
#elif defined(i386) || defined(__i386) || defined(__i386__)
#define ARCH_X86 1
#elif defined(__aarch64__)
#define ARCH_ARM64 1
#elif defined(__arm__)
#define ARCH_ARM32 1
#else
#error Architecture not supported.
#endif
#else
#error Compiler not supported
#endif

#if defined(ARCH_X64)
#define ARCH_64BIT 1
#elif defined(ARCH_X86)
#define ARCH_32BIT 1
#endif

#define MAX(A, B) (((A) > (B)) ? (A) : (B))
#define MIN(A, B) (((A) < (B)) ? (A) : (B))

#define KB(A) ((U64)(A) << 10)
#define MB(A) ((U64)(A) << 20)
#define GB(A) ((U64)(A) << 30)

#define Kil(A) ((U64)(A) * 1000)
#define Mil(A) ((U64)(A) * 1000000)
#define Bil(A) ((U64)(A) * 1000000000)

#define ToBool(A)        (((A) != 0) ? (1) : (0))
#define AlignOf(A, B)    (((A) + (B) - 1) & (~((B) - 1)))
#define ToCeilInt(A, B)  (((A) + (B - 1)) / (B))
#define StaticArrSize(A) (sizeof(A) / sizeof((A)[0]))

#define BitFieldGet(arr, i)   (!!((arr)[(i) / (sizeof((arr)[0]) * 8)] & (1 << ((i) % (sizeof((arr)[0]) * 8)))))
#define BitFieldSet(arr, i)   ((arr)[(i) / (sizeof((arr)[0]) * 8)] |= (1 << ((i) % (sizeof((arr)[0]) * 8))))
#define BitFieldReset(arr, i) ((arr)[(i) / (sizeof((arr)[0]) * 8)] &= ~(1 << ((i) % (sizeof((arr)[0]) * 8))))

#define DeferLoop(begin, end) for (int _i_ = ((begin), 0); !_i_; _i_ += 1, (end))
#define ArenaScoped(arena)    for (struct { Temp t; int i; } __it = {.t = temp_begin(arena), .i = 0}; !__it.i; (__it.i += 1, temp_end(__it.t)))

#define MemoryCopy(dst, src, size) memmove((dst), (src), (size))
#define MemoryCopyArray(d, s)      MemoryCopy((d), (s), sizeof(d))

#define MemoryZero(p, s)    memset((p), 0, (s))
#define MemoryZeroStruct(A) MemoryZero((A), sizeof(*(A)))

#define _glue(a, b) a##b
#define Glue(A, B)  _glue(A, B)

#define Stringify_(S) #S
#define Stringify(S)  Stringify_(S)

#if WCHAR_MAX > 0xffffu
#define WCHAR_UTF32
#else
#define WCHAR_UTF16
#endif

#if defined(COMPILER_MSVC)
#define TRAP() __debugbreak()
#elif defined(COMPILER_GCC) || defined(COMPILER_CLANG)
#define TRAP() __builtin_trap()
#endif

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

#define S32_MAX 0x7FFFFFFFul

#define U32_MAX 0xFFFFFFFFul
#define U64_MAX 0xFFFFFFFFFFFFFFFFull

enum ByteUnit
{
    Byte,
    KiByte,
    MiByte,
    GiByte,
    TiByte,
    PiByte,
};

struct ByteSize
{
    F32 value;
    ByteUnit unit;
};

enum Month
{
    Month_Jan,
    Month_Feb,
    Month_Mar,
    Month_Apr,
    Month_May,
    Month_Jun,
    Month_Jul,
    Month_Aug,
    Month_Sep,
    Month_Oct,
    Month_Nov,
    Month_Dec,
};

struct Date
{
    U32 date;
    Month month;
    S32 year;

    // NOTE: Later, maybe add time also?
    // U32 hour;
    // U32 minute;
    // U32 second;
};

enum ResultDomain
{
    Domain_None,
    Domain_OS,
    Domain_App,
    Domain_Network,
    Domain_YAML,
};

struct Result
{
    B32 success;
    ResultDomain domain;
    U32 code;
    const char *context;
};

#define ResultSuccess() {.success = 1}
#define CheckAndClearResult(res)   \
    do                             \
    {                              \
        if (!(res).success)        \
            goto Cleanup;          \
        else                       \
            res = ResultSuccess(); \
    } while (0)
#define ClearResult(res) ((res) && ((*(res) = ResultSuccess()), 1))

#if COMPILER_MSVC
#include <intrin.h>
#if ARCH_64BIT
#define ins_atomic_u128_eval_cond_assign(x, k, c) (B32) InterlockedCompareExchange128((__int64 *)(x), ((__int64 *)&(k))[1], ((__int64 *)&(k))[0], (__int64 *)c)
#define ins_atomic_u64_eval(x)                    *((volatile U64 *)(x))
#define ins_atomic_u64_inc_eval(x)                InterlockedIncrement64((__int64 *)(x))
#define ins_atomic_u64_dec_eval(x)                InterlockedDecrement64((__int64 *)(x))
#define ins_atomic_u64_eval_assign(x, c)          InterlockedExchange64((__int64 *)(x), (c))
#define ins_atomic_u64_add_eval(x, c)             InterlockedAdd64((__int64 *)(x), c)
#define ins_atomic_u64_eval_cond_assign(x, k, c)  InterlockedCompareExchange64((__int64 *)(x), (k), (c))
#define ins_atomic_u32_eval(x)                    *((volatile U32 *)(x))
#define ins_atomic_u32_inc_eval(x)                InterlockedIncrement((LONG *)(x))
#define ins_atomic_u32_dec_eval(x)                InterlockedDecrement((LONG *)(x))
#define ins_atomic_u32_eval_assign(x, c)          InterlockedExchange((LONG *)(x), (c))
#define ins_atomic_u32_eval_cond_assign(d, e, c)  InterlockedCompareExchange((LONG *)(d), (e), (c))
#define ins_atomic_u32_add_eval(x, c)             InterlockedAdd((LONG *)(x), (c))
#define ins_atomic_u8_eval_assign(x, c)           InterlockedExchange8((CHAR *)(x), (c))
#else
#error Atomic intrinsics not defined for this compiler / architecture combination.
#endif
#elif COMPILER_CLANG || COMPILER_GCC
#define ins_atomic_u128_eval_cond_assign(x, k, c) (B32) __atomic_compare_exchange_n((__int128 *)(x), (__int128 *)(c), *(__int128 *)(k), 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
#define ins_atomic_u64_eval(x)                    __atomic_load_n(x, __ATOMIC_SEQ_CST)
#define ins_atomic_u64_inc_eval(x)                (__atomic_fetch_add((U64 *)(x), 1, __ATOMIC_SEQ_CST) + 1)
#define ins_atomic_u64_dec_eval(x)                (__atomic_fetch_sub((U64 *)(x), 1, __ATOMIC_SEQ_CST) - 1)
#define ins_atomic_u64_eval_assign(x, c)          __atomic_exchange_n(x, c, __ATOMIC_SEQ_CST)
#define ins_atomic_u64_add_eval(x, c)             (__atomic_fetch_add((U64 *)(x), c, __ATOMIC_SEQ_CST) + (c))
#define ins_atomic_u64_eval_cond_assign(x, k, c)  ({ U64 _new = (c); __atomic_compare_exchange_n((U64 *)(x),&_new,(k),0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST); _new; })
#define ins_atomic_u32_eval(x)                    __atomic_load_n(x, __ATOMIC_SEQ_CST)
#define ins_atomic_u32_inc_eval(x)                (__atomic_fetch_add((U32 *)(x), 1, __ATOMIC_SEQ_CST) + 1)
#define ins_atomic_u32_dec_eval(x)                (__atomic_fetch_sub((U32 *)(x), 1, __ATOMIC_SEQ_CST) - 1)
#define ins_atomic_u32_add_eval(x, c)             (__atomic_fetch_add((U32 *)(x), c, __ATOMIC_SEQ_CST) + (c))
#define ins_atomic_u32_eval_assign(x, c)          __atomic_exchange_n((x), (c), __ATOMIC_SEQ_CST)
#define ins_atomic_u32_eval_cond_assign(d, e, c)  ({ U32 _new = (c); __atomic_compare_exchange_n((U32 *)(d),&_new,(e),0,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST); _new; })
#define ins_atomic_u8_eval_assign(x, c)           __atomic_exchange_n((x), (c), __ATOMIC_SEQ_CST)
#else
#error Atomic intrinsics not defined for this compiler / architecture.
#endif

#if ARCH_64BIT
#define ins_atomic_ptr_eval_cond_assign(x, k, c) (void *)ins_atomic_u64_eval_cond_assign((U64 *)(x), (U64)(k), (U64)(c))
#define ins_atomic_ptr_eval_assign(x, c)         (void *)ins_atomic_u64_eval_assign((U64 *)(x), (U64)(c))
#define ins_atomic_ptr_eval(x)                   (void *)ins_atomic_u64_eval((U64 *)x)
#else
#error Atomic intrinsics for pointers not defined for this architecture.
#endif

#if ARCH_64BIT
#define IntFromPtr(ptr) ((U64)(ptr))
#elif ARCH_32BIT
#define IntFromPtr(ptr) ((U32)(ptr))
#else
#error Missing pointer-to-integer cast for this architecture.
#endif

#if DBG
#if OS_WINDOWS

#if MSCBL_CORE
#define MSCBL_API extern "C" __declspec(dllexport)
#else // MSCBL_CORE
#define MSCBL_API extern "C" __declspec(dllimport)
#endif // MSCBL_CORE

#define MSCBL_EXP extern "C" __declspec(dllexport)
#elif OS_LINUX

#define MSCBL_API extern "C"
#define MSCBL_EXP extern "C"

#endif // OS
#else  // DBG
#define MSCBL_API extern
#define MSCBL_EXP extern
#endif // DBG

void bytes_as_hex_lower(U8 *data, U64 start, U64 len, char *out);
void bytes_as_hex_upper(U8 *data, U64 start, U64 len, char *out);
U64 date_to_timestamp(Date date);
Date timestamp_to_date(U64 timestamp);
MSCBL_API U32 month_days(Date date);

// TODO: Move these to some other place
#define APP_NAME Miscible

// #define MODEL_PATH  ROOT_DIR "/CLIP-ViT-B-32-laion2B-s34B-b79K.gguf"
#define ATLAS_DIR   "atlas"
#define DB_FILE     "miscible.sqlite"
#define CONFIG_FILE "miscible.yaml"

#define ATLAS_CAPACITY 100
#define ATLAS_SIZE     2560
#define ATLAS_CHANNELS 4
#define THUMB_PER_SIDE 10
#define THUMB_SIZE     256
