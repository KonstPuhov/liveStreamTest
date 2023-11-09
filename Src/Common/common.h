#pragma once
#include <stdarg.h>
#include <stdio.h>
#include <errno.h>

#ifndef _VERSION_
#define _VERSION_ "0.1"
#endif

typedef unsigned char byte;

static inline const char* _version() {
	int minor, major, build;
	static char str[32];
	if(sscanf(_VERSION_,"%d.%d-%d", &minor,&major,&build)==3) {
		snprintf(str, 31, "v%d.%d.%d", minor, major, build);
		return str;
	}
	return _VERSION_;
}
static inline void printVersion() {
	puts(_version());
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

//  Расчёт задержки (us) между фреймами заданного размера для получения заданного битрейта(кБит/сек)
static inline useconds_t bitrate2usecs(uint bitrate, size_t frameSz) {
	return 1000000*frameSz / (bitrate*1000/8);
}