#ifndef _STDDEF_H
#define _STDDEF_H

typedef long ptrdiff_t;
typedef unsigned long size_t;
typedef struct { long long __ll; long double __ld; } max_align_t;
typedef int wchar_t;

#define NULL ((void*)0)
#define offsetof(type, member) ((size_t)( (char *)&(((type *)0)->member) - (char *)0 ))

#endif
