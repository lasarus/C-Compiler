#ifndef _STDARG_H
#define _STDARG_H

#define va_start(v, l) __builtin_va_start(v, l)
#define va_end(v) __builtin_va_end(v)
#define va_arg(v, l) __builtin_va_arg(v, l)
#define va_copy(d, s) __builtin_va_copy(d, s)

// I don't really know why this is necessary for glibc headers
// to work. But I defined it anyways.
typedef __builtin_va_list va_list;
typedef __builtin_va_list __gnuc_va_list;

#endif
