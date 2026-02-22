#pragma GCC optimize("-Os")
#include <ogc/system.h>
#include <stdarg.h>
#include <unistd.h>

#define NANOPRINTF_USE_FIELD_WIDTH_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_PRECISION_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_FLOAT_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_LARGE_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_SMALL_FORMAT_SPECIFIERS 1
#define NANOPRINTF_USE_BINARY_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_WRITEBACK_FORMAT_SPECIFIERS 0
#define NANOPRINTF_USE_ALT_FORM_FLAG 1

#define NANOPRINTF_VISIBILITY_STATIC
#define NANOPRINTF_IMPLEMENTATION
#include "nanoprintf.h"

void kprintf(const char *fmt, ...)
{
	char buf[256];
	int n;

	va_list args;
	va_start(args, fmt);
	n = npf_vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	write(STDERR_FILENO, buf, n);
}
