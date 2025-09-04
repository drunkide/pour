#ifndef STDINT_H
#define STDINT_H

#include <stddef.h>
#include <sys/types.h>

#ifndef INT64_MIN
#define INT64_MIN ((int64_t)0x8000000000000000ULL)
#define INT64_MAX ((int64_t)0x7FFFFFFFFFFFFFFFULL)
#define UINT64_MAX ((uint64_t)0xFFFFFFFFFFFFFFFFULL)
#endif

#ifdef __STRICT_ANSI__
__extension__ typedef long long int64_t;
__extension__ typedef unsigned long long uint64_t;
#endif

typedef ptrdiff_t intptr_t;

#endif
