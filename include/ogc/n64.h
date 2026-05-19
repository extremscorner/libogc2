/*-------------------------------------------------------------

n64.h -- N64 controller subsystem

Copyright (C) 2019 - 2026 Extrems' Corner.org

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

#ifndef __OGC_N64_H__
#define __OGC_N64_H__

#include <gctypes.h>

#define N64_ERR_READY           0
#define N64_ERR_NO_CONTROLLER  -1
#define N64_ERR_BUSY           -2
#define N64_ERR_TRANSFER       -3

#define N64_BUTTON_RIGHT   0x0001
#define N64_BUTTON_LEFT    0x0002
#define N64_BUTTON_DOWN    0x0004
#define N64_BUTTON_UP      0x0008
#define N64_BUTTON_START   0x0010
#define N64_BUTTON_Z       0x0020
#define N64_BUTTON_B       0x0040
#define N64_BUTTON_A       0x0080
#define N64_BUTTON_C_RIGHT 0x0100
#define N64_BUTTON_C_LEFT  0x0200
#define N64_BUTTON_C_DOWN  0x0400
#define N64_BUTTON_C_UP    0x0800
#define N64_BUTTON_R       0x1000
#define N64_BUTTON_L       0x2000

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*N64Callback)(s32 chan, s32 result);

typedef struct N64Status {
	u16 button;
	s8 stickX;
	s8 stickY;
	s8 err;
} N64Status;

s32 N64_GetStatus(s32 chan, u8 *status);
s32 N64_GetStatusAsync(s32 chan, u8 *status, N64Callback callback);
s32 N64_Reset(s32 chan, u8 *status);
s32 N64_ResetAsync(s32 chan, u8 *status, N64Callback callback);
s32 N64_Read(s32 chan, N64Status *status);
s32 N64_ReadAsync(s32 chan, N64Status *status, N64Callback callback);

#ifdef __cplusplus
}
#endif

#endif
