#ifndef STDBOOL_H
#define STDBOOL_H

/* C++ and C23 have native bool type */
#if !defined(__cplusplus) && (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L)
#define __bool_true_false_are_defined 1
#define false 0
#define true 1
typedef unsigned char bool;
#endif

#endif
