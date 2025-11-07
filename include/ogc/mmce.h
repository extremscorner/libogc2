/*-------------------------------------------------------------

mmce.h -- MMCE support

Copyright (C) 2022 - 2025 Extrems' Corner.org

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

#ifndef __OGC_MMCE_H__
#define __OGC_MMCE_H__

#include <gctypes.h>
#include <ogc/disc_io.h>
#include <ogc/dvd.h>

#define MMCE_RESULT_READY        0
#define MMCE_RESULT_BUSY        -1
#define MMCE_RESULT_WRONGDEVICE -2
#define MMCE_RESULT_NOCARD      -3
#define MMCE_RESULT_VERSION     -4
#define MMCE_RESULT_FATAL_ERROR -128

#ifdef __cplusplus
extern "C" {
#endif

enum {
	MMCE_MODE_READONLY = 0,
	MMCE_MODE_READWRITE
};

s32 MMCE_ProbeEx(s32 chan);
s32 MMCE_GetDeviceID(s32 chan, u32 *id);
s32 MMCE_GetAccessMode(s32 chan, u8 *mode);
s32 MMCE_SetAccessMode(s32 chan, u8 mode);
s32 MMCE_GetDiskID(s32 chan, dvddiskid *diskID);
s32 MMCE_SetDiskID(s32 chan, const dvddiskid *diskID);
s32 MMCE_GetDiskInfo(s32 chan, char diskInfo[64]);
s32 MMCE_SetDiskInfo(s32 chan, const char diskInfo[64]);
s32 MMCE_GetGameID(s32 chan, char gameID[10]);
s32 MMCE_SetGameID(s32 chan, const char gameID[10]);
s32 MMCE_ReadSectors(s32 chan, u32 sector, u16 numSectors, void *buffer);
s32 MMCE_WriteSectors(s32 chan, u32 sector, u16 numSectors, const void *buffer);

#define DEVICE_TYPE_GAMECUBE_MMCE(x) (('G'<<24)|('M'<<16)|('C'<<8)|('0'+(x)))

extern DISC_INTERFACE __io_mmcea;
extern DISC_INTERFACE __io_mmceb;
extern DISC_INTERFACE __io_mmce2;

#ifdef __cplusplus
}
#endif

#endif
