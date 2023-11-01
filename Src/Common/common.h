#pragma once
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>

#define _VERSION_ "0.1"

typedef unsigned char byte;

static inline void printVersion() {
	puts(_VERSION_);
}

// вывод отладки в stderr
static inline void log(const char* format, ...)
{
	va_list p;
	va_start(p, format);
	vfprintf(stderr, format, p);
}
#ifdef DEBUG
#define debug(...) log(__VA_ARGS__)
#else
#define debug(...)
#endif