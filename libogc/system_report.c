/*-------------------------------------------------------------

system_report.c -- Formatted output to serial port

Copyright (C) 2025 Extrems' Corner.org

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any
damages arising from the use of this software.

Permission is granted to anyone to use this software for any
purpose, including commercial applications, and to alter it and
redistribute it freely, subject to the following restrictions:

1.	The origin of this software must not be misrepresented; you
must not claim that you wrote the original software. If you use
this software in a product, an acknowledgment in the product
documentation would be appreciated but is not required.

2.	Altered source versions must be plainly marked as such, and
must not be misrepresented as being the original software.

3.	This notice may not be removed or altered from any source
distribution.

-------------------------------------------------------------*/

#include <gctypes.h>
#include <ogc/system.h>
#include <ogc/usbgecko.h>
#include <stdarg.h>
#include <stdio.h>

static FILE *fp = NULL;
static s32 Chan = -1;
static bool Safe = true;

extern s32 InitializeUART(void);
extern s32 WriteUARTN(void *buf, u32 len);

static int WriteReport(void *c, const char *buf, int n)
{
	if (usb_isgeckoalive(Chan)) {
		if (Safe)
			return usb_sendbuffer_safe(Chan, buf, n);
		else
			return usb_sendbuffer(Chan, buf, n);
	} else {
		if (InitializeUART() != 0)
			return -1;
		if (WriteUARTN((void *)buf, n) != 0)
			return -1;
	}

	return n;
}

static void __attribute__((constructor)) __SYS_InitReport(void)
{
	fp = fwopen(NULL, WriteReport);
	setlinebuf(fp);
}

static void __attribute__((destructor)) __SYS_DeinitReport(void)
{
	fclose(fp);
	fp = NULL;
}

void __attribute__((weak)) SYS_Report(const char *msg, ...)
{
	va_list list;
	va_start(list, msg);
	vfprintf(fp, msg, list);
	va_end(list);
}

void __attribute__((weak)) SYS_Reportv(const char *msg, va_list list)
{
	vfprintf(fp, msg, list);
}

void SYS_STDIO_Report(bool use_stdout)
{
	fflush(stderr);
	stderr = fp;

	if (use_stdout) {
		fflush(stdout);
		stdout = fp;
	}
}

void SYS_EnableGecko(s32 chan, bool safe)
{
	flockfile(fp);
	Chan = chan;
	Safe = safe;
	funlockfile(fp);
}
