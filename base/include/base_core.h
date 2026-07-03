#ifndef BASE_CORE_H
#define BASE_CORE_H

#include <stddef.h>
#include <stdint.h>

// ---------------------------------------------------------------------------
// Sized integer types
// ---------------------------------------------------------------------------

typedef uint8_t U8;
typedef uint16_t U16;
typedef uint32_t U32;
typedef uint64_t U64;
typedef int8_t I8;
typedef int16_t I16;
typedef int32_t I32;
typedef int64_t I64;
typedef int32_t B32;
typedef float F32;
typedef double F64;

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

#define U8_MAX UINT8_MAX
#define U16_MAX UINT16_MAX
#define U32_MAX UINT32_MAX
#define U64_MAX UINT64_MAX
#define I8_MIN INT8_MIN
#define I8_MAX INT8_MAX
#define I16_MIN INT16_MIN
#define I16_MAX INT16_MAX
#define I32_MIN INT32_MIN
#define I32_MAX INT32_MAX
#define I64_MIN INT64_MIN
#define I64_MAX INT64_MAX

// ---------------------------------------------------------------------------
// Utility macros (parameterized = PascalCase, constants = UPPER_CASE)
// ---------------------------------------------------------------------------

#define ArrayCount(a) (sizeof(a) / sizeof((a)[0]))
#define AlignUp(x, a) (((x) + ((a) - 1)) & ~((a) - 1))
#define AlignDown(x, a) ((x) & ~((a) - 1))
#define Min(a, b) ((a) < (b) ? (a) : (b))
#define Max(a, b) ((a) > (b) ? (a) : (b))
#define Clamp(lo, x, hi) (Min(Max((x), (lo)), (hi)))

#define Kilobytes(n) ((U64)(n) << 10)
#define Megabytes(n) ((U64)(n) << 20)
#define Gigabytes(n) ((U64)(n) << 30)

#define Unused(x) ((void)(x))

#define OffsetOf(T, member) ((U64)offsetof(T, member))
#define ContainerOf(ptr, T, member) ((T *)((U8 *)(ptr) - OffsetOf(T, member)))

// ---------------------------------------------------------------------------
// Platform detection
// ---------------------------------------------------------------------------

#if defined(_WIN32)
#define OS_WINDOWS 1
#define OS_LINUX 0
#define OS_MACOS 0
#elif defined(__linux__)
#define OS_WINDOWS 0
#define OS_LINUX 1
#define OS_MACOS 0
#elif defined(__APPLE__)
#define OS_WINDOWS 0
#define OS_LINUX 0
#define OS_MACOS 1
#else
#error "Unsupported platform"
#endif

// ---------------------------------------------------------------------------
// Compiler detection
// ---------------------------------------------------------------------------

#if defined(__GNUC__) || defined(__clang__)
#define COMPILER_GCC 1
#define COMPILER_MSVC 0
#define THREAD_LOCAL _Thread_local
#elif defined(_MSC_VER)
#define COMPILER_GCC 0
#define COMPILER_MSVC 1
#define THREAD_LOCAL __declspec(thread)
#else
#error "Unsupported compiler"
#endif

#endif // BASE_CORE_H
