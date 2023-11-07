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
#define log(...) _inl_log("LOG:",__FILE__,__LINE__,__VA_ARGS__)
#define errlog(...) _inl_log("ERROR:",__FILE__,__LINE__,__VA_ARGS__)

static inline void _inl_log(const char *prefix, const char* file, int line, const char* format, ...)
{
	va_list p;
	va_start(p, format);
	fprintf(stderr, "%s:%s::%d: ", prefix, file, line);
	vfprintf(stderr, format, p);
}
#ifdef DEBUG
#define debug(...) _inl_log("DEBUG:",__FILE__,__LINE__,__VA_ARGS__)
#else
#define debug(...)
#endif