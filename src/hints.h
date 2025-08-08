#ifndef COTTONTAIL_SRC_HINTS_H_
#define COTTONTAIL_SRC_HINTS_H_

#if defined(__has_builtin)
#if __has_builtin(__builtin_expect)
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#endif
#elif defined(__GNUC__) || defined(__clang__)
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#endif

#endif
