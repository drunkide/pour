#ifndef COMMON_COMMON_H
#define COMMON_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <lua.h>
#include <lauxlib.h>

#define STRUCT(X) \
    struct X; \
    typedef struct X X; \
    struct X

#define DONT_WARN_UNUSED(X) \
    ((void)(X))

#endif
