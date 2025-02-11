/*-------------------------------------------------------------

dvd.h -- DVD subsystem

Copyright (C) 2004
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)

Additionally following copyrights apply for the patching system:
 * Copyright (C) 2005 The GameCube Linux Team
 * Copyright (C) 2005 Albert Herranz

Thanks alot guys for that incredible patch!!

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
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include "asm.h"
#include "processor.h"
#include "cache.h"
#include "lwp.h"
#include "irq.h"
#include "ogcsys.h"
#include "system.h"
#include "dvd.h"
#include "timesupp.h"

//#define _DVD_DEBUG

#define DVD_BRK							(1<<0)
#define DVD_DE_MSK						(1<<1)
#define DVD_DE_INT						(1<<2)
#define DVD_TC_MSK						(1<<3)
#define DVD_TC_INT						(1<<4)
#define DVD_BRK_MSK						(1<<5)
#define DVD_BRK_INT						(1<<6)

#define DVD_CVR_INT						(1<<2)
#define DVD_CVR_MSK						(1<<1)
#define DVD_CVR_STATE					(1<<0)

#define DVD_DI_MODE						(1<<2)
#define DVD_DI_DMA						(1<<1)
#define DVD_DI_START					(1<<0)

#define DVD_DISKIDSIZE					0x20
#define DVD_DRVINFSIZE					0x20
#define DVD_GCODE_BLKSIZE				0x200

#define DVD_INQUIRY						0x12000000
#define DVD_FWSETOFFSET					0x32000000
#define DVD_FWENABLEEXT					0x55000000
#define DVD_READ						0xA8000000
#define DVD_READDISKID					0xA8000040
#define DVD_SEEK						0xAB000000
#define DVD_GCODE_READ					0xB2000000
#define DVD_GCODE_WRITEBUFFER			0xB9000000
#define DVD_GCODE_WRITE					0xBA000000
#define DVD_GCODE_WRITEBUFFEREX			0xBC000000
#define DVD_GCODE_WRITEEX				0xBD000000
#define DVD_REQUESTERROR				0xE0000000
#define DVD_AUDIOSTREAM					0xE1000000
#define DVD_AUDIOSTATUS					0xE2000000
#define DVD_STOPMOTOR					0xE3000000
#define DVD_AUDIOCONFIG					0xE4000000
#define DVD_FWSETSTATUS					0xEE000000
#define DVD_FWWRITEMEM					0xFE010100
#define DVD_FWREADMEM					0xFE010000
#define DVD_FWCTRLMOTOR					0xFE110000
#define DVD_FWFUNCCALL					0xFE120000

#define DVD_MODEL04						0x20020402
#define DVD_MODEL06						0x20010608
#define DVD_MODEL08						0x20020823
#define DVD_MODEL08Q					0x20010831

#define DVD_FWIRQVECTOR					0x00804c

#define DVD_DRIVERESET					0x00000001
#define DVD_CHIPPRESENT					0x00000002
#define DVD_INTEROPER					0x00000004

/* drive status, status */
#define DVD_STATUS(s)					((u8)((s)>>24))

#define DVD_STATUS_READY				0x00
#define DVD_STATUS_COVER_OPENED			0x01
#define DVD_STATUS_DISK_CHANGE			0x02
#define DVD_STATUS_NO_DISK				0x03
#define DVD_STATUS_MOTOR_STOP			0x04
#define DVD_STATUS_DISK_ID_NOT_READ		0x05

/* drive status, error */
#define DVD_ERROR(s)					((u32)((s)&0x00ffffff))

#define DVD_ERROR_NO_ERROR				0x000000
#define DVD_ERROR_MOTOR_STOPPED			0x020400
#define DVD_ERROR_DISK_ID_NOT_READ		0x020401
#define DVD_ERROR_MEDIUM_NOT_PRESENT	0x023a00
#define DVD_ERROR_SEEK_INCOMPLETE		0x030200
#define DVD_ERROR_UNRECOVERABLE_READ	0x031100
#define DVD_ERROR_TRANSFER_PROTOCOL		0x040800
#define DVD_ERROR_INVALID_COMMAND		0x052000
#define DVD_ERROR_AUDIOBUFFER_NOTSET	0x052001
#define DVD_ERROR_BLOCK_OUT_OF_RANGE	0x052100
#define DVD_ERROR_INVALID_FIELD			0x052400
#define DVD_ERROR_INVALID_AUDIO_CMD		0x052401
#define DVD_ERROR_INVALID_CONF_PERIOD	0x052402
#define DVD_ERROR_END_OF_USER_AREA		0x056300
#define DVD_ERROR_MEDIUM_CHANGED		0x062800
#define DVD_ERROR_MEDIUM_CHANGE_REQ		0x0B5A01

#define DVD_SPINMOTOR_MASK				0x0000ff00

#define cpu_to_le32(x)					(((x>>24)&0x000000ff) | ((x>>8)&0x0000ff00) | ((x<<8)&0x00ff0000) | ((x<<24)&0xff000000))
#define dvd_may_retry(s)				(DVD_STATUS(s) == DVD_STATUS_READY || DVD_STATUS(s) == DVD_STATUS_DISK_ID_NOT_READ)

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))

typedef void (*dvdcallbacklow)(s32);
typedef void (*dvdstatecb)(dvdcmdblk *);

typedef struct _dvdcmdl {
	s32 cmd;
	void *buf;
	u32 len;
	s64 offset;
	dvdcallbacklow cb;
} dvdcmdl;

typedef struct _dvdcmds {
	void *buf;
	u32 len;
	s64 offset;
} dvdcmds;

static u32 __dvd_initflag = 0;
static u32 __dvd_stopnextint = 0;
static vu32 __dvd_resetoccured = 0;
static u32 __dvd_waitcoverclose = 0;
static u32 __dvd_breaking = 0;
static vu32 __dvd_resetrequired = 0;
static u32 __dvd_canceling = 0;
static u32 __dvd_pauseflag = 0;
static u32 __dvd_pausingflag = 0;
static u64 __dvd_lastresetend = 0;
static u32 __dvd_ready = 0;
static u32 __dvd_resumefromhere = 0;
static u32 __dvd_fatalerror = 0;
static u32 __dvd_lasterror = 0;
static u32 __dvd_internalretries = 0;
static u32 __dvd_autofinishing = 0;
static u32 __dvd_autoinvalidation = 1;
static u32 __dvd_cancellasterror = 0;
static u32 __dvd_drivechecked = 0;
static u32 __dvd_drivestate = 0;
static u32 __dvd_extensionsenabled = TRUE;
static u16 __dvd_gcode_writebufsize = 1;
static u32 __dvd_lastlen;
static u32 __dvd_nextcmdnum;
static u32 __dvd_workaround;
static u32 __dvd_workaroundseek;
static u32 __dvd_lastcmdwasread;
static u32 __dvd_currcmd;
static u32 __dvd_motorcntrl;
static lwpq_t __dvd_wait_queue;
static syswd_t __dvd_timeoutalarm;
static dvdcmdblk __dvd_block$15;
static dvdcmdblk __dvd_dummycmdblk;
static dvddiskid __dvd_tmpid0 ATTRIBUTE_ALIGN(32);
static dvddrvinfo __dvd_driveinfo ATTRIBUTE_ALIGN(32);
static dvdcallbacklow __dvd_callback = NULL;
static dvdcallbacklow __dvd_resetcovercb = NULL;
static dvdcallbacklow __dvd_finalunlockcb = NULL;
static dvdcallbacklow __dvd_finalreadmemcb = NULL;
static dvdcallbacklow __dvd_finalsudcb = NULL;
static dvdcallbacklow __dvd_finalstatuscb = NULL;
static dvdcallbacklow __dvd_finaladdoncb = NULL;
static dvdcallbacklow __dvd_finalpatchcb = NULL;
static dvdcallbacklow __dvd_finaloffsetcb = NULL;
static dvdcbcallback __dvd_cancelcallback = NULL;
static dvdcbcallback __dvd_mountusrcb = NULL;
static dvdstatecb __dvd_laststate = NULL;
static dvdcmdblk *__dvd_executing = NULL;
static void *__dvd_usrdata = NULL;
static dvddiskid *__dvd_diskID = (dvddiskid*)0x80000000;

static lwp_queue __dvd_waitingqueue[4];
static dvdcmdl __dvd_cmdlist[4];
static dvdcmds __dvd_cmd_curr,__dvd_cmd_prev;

static u32 __dvdpatchcode_size = 0;
static const u8 *__dvdpatchcode = NULL;

static const u32 __dvd_patchcode04_size = 448;
static const u8 __dvd_patchcode04[] =
{
	0xf7,0x10,0xff,0xf7,0xf4,0x74,0x25,0xd0,0x40,0xf7,0x20,0x4c,0x80,0xf4,0x74,0xd6,
	0x9c,0x08,0xf7,0x20,0xd6,0xfc,0xf4,0x74,0x28,0xae,0x08,0xf7,0x20,0xd2,0xfc,0x80,
	0x0c,0xc4,0xda,0xfc,0xfe,0xc8,0xda,0xfc,0xf5,0x00,0x01,0xe8,0x03,0xfc,0xc1,0x00,
	0xa0,0xf4,0x74,0x09,0xec,0x40,0x10,0xc8,0xda,0xfc,0xf5,0x00,0x02,0xe8,0x03,0xfc,
	0xbc,0x00,0xf4,0x74,0xf9,0xec,0x40,0x80,0x02,0xf0,0x20,0xc8,0x84,0x80,0xc0,0x9c,
	0x81,0xdc,0xb4,0x80,0xf5,0x30,0x00,0xf4,0x44,0xa1,0xd1,0x40,0xf8,0xaa,0x00,0x10,
	0xf4,0xd0,0x9c,0xd1,0x40,0xf0,0x01,0xdc,0xb4,0x80,0xf5,0x30,0x00,0xf7,0x48,0xaa,
	0x00,0xe9,0x07,0xf4,0xc4,0xa1,0xd1,0x40,0x10,0xfe,0xd8,0x32,0xe8,0x1d,0xf7,0x48,
	0xa8,0x00,0xe8,0x28,0xf7,0x48,0xab,0x00,0xe8,0x22,0xf7,0x48,0xe1,0x00,0xe8,0x1c,
	0xf7,0x48,0xee,0x00,0xe8,0x3d,0xd8,0x55,0xe8,0x31,0xfe,0x71,0x04,0xfd,0x22,0x00,
	0xf4,0x51,0xb0,0xd1,0x40,0xa0,0x40,0x04,0x40,0x06,0xea,0x33,0xf2,0xf9,0xf4,0xd2,
	0xb0,0xd1,0x40,0x71,0x04,0xfd,0x0a,0x00,0xf2,0x49,0xfd,0x05,0x00,0x51,0x04,0xf2,
	0x36,0xfe,0xf7,0x21,0xbc,0xff,0xf7,0x31,0xbc,0xff,0xfe,0xf5,0x30,0x01,0xfd,0x7e,
	0x00,0xea,0x0c,0xf5,0x30,0x01,0xc4,0xb0,0x81,0xf5,0x30,0x02,0xc4,0x94,0x81,0xdc,
	0xb4,0x80,0xf8,0xe0,0x00,0x10,0xa0,0xf5,0x10,0x01,0xf5,0x10,0x02,0xf5,0x10,0x03,
	0xfe,0xc8,0xda,0xfc,0xf7,0x00,0xfe,0xff,0xf7,0x31,0xd2,0xfc,0xea,0x0b,0xc8,0xda,
	0xfc,0xf7,0x00,0xfd,0xff,0xf7,0x31,0xd6,0xfc,0xc4,0xda,0xfc,0xcc,0x44,0xfc,0xf7,
	0x00,0xfe,0xff,0xc4,0x44,0xfc,0xf4,0x7d,0x28,0xae,0x08,0xe9,0x07,0xf4,0x75,0x60,
	0xd1,0x40,0xea,0x0c,0xf4,0x7d,0xd6,0x9c,0x08,0xe9,0x05,0xf4,0x75,0x94,0xd1,0x40,
	0xf2,0x7c,0xd0,0x04,0xcc,0x5b,0x80,0xd8,0x01,0xe9,0x02,0x7c,0x04,0x51,0x20,0x71,
	0x34,0xf4,0x7d,0xc1,0x85,0x08,0xe8,0x05,0xfe,0x80,0x01,0xea,0x02,0x80,0x00,0xa5,
	0xd8,0x00,0xe8,0x02,0x85,0x0c,0xc5,0xda,0xfc,0xf4,0x75,0xa0,0xd1,0x40,0x14,0xfe,
	0xf7,0x10,0xff,0xf7,0xf4,0xc9,0xa0,0xd1,0x40,0xd9,0x00,0xe8,0x22,0x21,0xf7,0x49,
	0x08,0x06,0xe9,0x05,0x85,0x02,0xf5,0x10,0x01,0xf4,0x79,0x00,0xf0,0x00,0xe9,0x05,
	0x80,0x00,0xf5,0x10,0x09,0xd9,0x06,0xe9,0x06,0x61,0x06,0xd5,0x06,0x41,0x06,0xf4,
	0xe0,0x9f,0xdc,0xc7,0xf4,0xe0,0xb5,0xcb,0xc7,0x00,0x00,0x00,0x74,0x0a,0x08,0x00,
	0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

static const u32 __dvd_patchcode06_size = 448;
static const u8 __dvd_patchcode06[] =
{
	0xf7,0x10,0xff,0xf7,0xf4,0x74,0x25,0xd0,0x40,0xf7,0x20,0x4c,0x80,0xf4,0x74,0x42,
	0x9d,0x08,0xf7,0x20,0xd6,0xfc,0xf4,0x74,0x45,0xb1,0x08,0xf7,0x20,0xd2,0xfc,0x80,
	0x0c,0xc4,0xda,0xfc,0xfe,0xc8,0xda,0xfc,0xf5,0x00,0x01,0xe8,0x03,0xfc,0xc1,0x00,
	0xa0,0xf4,0x74,0x09,0xec,0x40,0x10,0xc8,0xda,0xfc,0xf5,0x00,0x02,0xe8,0x03,0xfc,
	0xbc,0x00,0xf4,0x74,0x02,0xed,0x40,0x80,0x02,0xf0,0x20,0xc8,0x78,0x80,0xc0,0x90,
	0x81,0xdc,0xa8,0x80,0xf5,0x30,0x00,0xf4,0x44,0xa1,0xd1,0x40,0xf8,0xaa,0x00,0x10,
	0xf4,0xd0,0x9c,0xd1,0x40,0xf0,0x01,0xdc,0xa8,0x80,0xf5,0x30,0x00,0xf7,0x48,0xaa,
	0x00,0xe9,0x07,0xf4,0xc4,0xa1,0xd1,0x40,0x10,0xfe,0xd8,0x32,0xe8,0x1d,0xf7,0x48,
	0xa8,0x00,0xe8,0x28,0xf7,0x48,0xab,0x00,0xe8,0x22,0xf7,0x48,0xe1,0x00,0xe8,0x1c,
	0xf7,0x48,0xee,0x00,0xe8,0x3d,0xd8,0x55,0xe8,0x31,0xfe,0x71,0x04,0xfd,0x22,0x00,
	0xf4,0x51,0xb0,0xd1,0x40,0xa0,0x40,0x04,0x40,0x06,0xea,0x33,0xf2,0xf9,0xf4,0xd2,
	0xb0,0xd1,0x40,0x71,0x04,0xfd,0x0a,0x00,0xf2,0x49,0xfd,0x05,0x00,0x51,0x04,0xf2,
	0x36,0xfe,0xf7,0x21,0xbc,0xff,0xf7,0x31,0xbc,0xff,0xfe,0xf5,0x30,0x01,0xfd,0x7e,
	0x00,0xea,0x0c,0xf5,0x30,0x01,0xc4,0xa4,0x81,0xf5,0x30,0x02,0xc4,0x88,0x81,0xdc,
	0xa8,0x80,0xf8,0xe0,0x00,0x10,0xa0,0xf5,0x10,0x01,0xf5,0x10,0x02,0xf5,0x10,0x03,
	0xfe,0xc8,0xda,0xfc,0xf7,0x00,0xfe,0xff,0xf7,0x31,0xd2,0xfc,0xea,0x0b,0xc8,0xda,
	0xfc,0xf7,0x00,0xfd,0xff,0xf7,0x31,0xd6,0xfc,0xc4,0xda,0xfc,0xcc,0x44,0xfc,0xf7,
	0x00,0xfe,0xff,0xc4,0x44,0xfc,0xf4,0x7d,0x45,0xb1,0x08,0xe9,0x07,0xf4,0x75,0x60,
	0xd1,0x40,0xea,0x0c,0xf4,0x7d,0x42,0x9d,0x08,0xe9,0x05,0xf4,0x75,0x94,0xd1,0x40,
	0xf2,0x7c,0xd0,0x04,0xcc,0x5b,0x80,0xd8,0x01,0xe9,0x02,0x7c,0x04,0x51,0x20,0x71,
	0x34,0xf4,0x7d,0xb9,0x85,0x08,0xe8,0x05,0xfe,0x80,0x01,0xea,0x02,0x80,0x00,0xa5,
	0xd8,0x00,0xe8,0x02,0x85,0x0c,0xc5,0xda,0xfc,0xf4,0x75,0xa0,0xd1,0x40,0x14,0xfe,
	0xf7,0x10,0xff,0xf7,0xf4,0xc9,0xa0,0xd1,0x40,0xd9,0x00,0xe8,0x22,0x21,0xf7,0x49,
	0x08,0x06,0xe9,0x05,0x85,0x02,0xf5,0x10,0x01,0xf4,0x79,0x00,0xf0,0x00,0xe9,0x05,
	0x80,0x00,0xf5,0x10,0x09,0xd9,0x06,0xe9,0x06,0x61,0x06,0xd5,0x06,0x41,0x06,0xf4,
	0xe0,0xbc,0xdf,0xc7,0xf4,0xe0,0x37,0xcc,0xc7,0x00,0x00,0x00,0x74,0x0a,0x08,0x00,
	0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

static const u32 __dvd_patchcode08_size = 448;
static const u8 __dvd_patchcode08[] =
{
	0xf7,0x10,0xff,0xf7,0xf4,0x74,0x25,0xd0,0x40,0xf7,0x20,0x4c,0x80,0xf4,0x74,0x32,
	0x9d,0x08,0xf7,0x20,0xd6,0xfc,0xf4,0x74,0x75,0xae,0x08,0xf7,0x20,0xd2,0xfc,0x80,
	0x0c,0xc4,0xda,0xfc,0xfe,0xc8,0xda,0xfc,0xf5,0x00,0x01,0xe8,0x03,0xfc,0xc1,0x00,
	0xa0,0xf4,0x74,0x09,0xec,0x40,0x10,0xc8,0xda,0xfc,0xf5,0x00,0x02,0xe8,0x03,0xfc,
	0xbc,0x00,0xf4,0x74,0xf5,0xec,0x40,0x80,0x02,0xf0,0x20,0xc8,0x80,0x80,0xc0,0x98,
	0x81,0xdc,0xb0,0x80,0xf5,0x30,0x00,0xf4,0x44,0xa1,0xd1,0x40,0xf8,0xaa,0x00,0x10,
	0xf4,0xd0,0x9c,0xd1,0x40,0xf0,0x01,0xdc,0xb0,0x80,0xf5,0x30,0x00,0xf7,0x48,0xaa,
	0x00,0xe9,0x07,0xf4,0xc4,0xa1,0xd1,0x40,0x10,0xfe,0xd8,0x32,0xe8,0x1d,0xf7,0x48,
	0xa8,0x00,0xe8,0x28,0xf7,0x48,0xab,0x00,0xe8,0x22,0xf7,0x48,0xe1,0x00,0xe8,0x1c,
	0xf7,0x48,0xee,0x00,0xe8,0x3d,0xd8,0x55,0xe8,0x31,0xfe,0x71,0x04,0xfd,0x22,0x00,
	0xf4,0x51,0xb0,0xd1,0x40,0xa0,0x40,0x04,0x40,0x06,0xea,0x33,0xf2,0xf9,0xf4,0xd2,
	0xb0,0xd1,0x40,0x71,0x04,0xfd,0x0a,0x00,0xf2,0x49,0xfd,0x05,0x00,0x51,0x04,0xf2,
	0x36,0xfe,0xf7,0x21,0xbc,0xff,0xf7,0x31,0xbc,0xff,0xfe,0xf5,0x30,0x01,0xfd,0x7e,
	0x00,0xea,0x0c,0xf5,0x30,0x01,0xc4,0xac,0x81,0xf5,0x30,0x02,0xc4,0x90,0x81,0xdc,
	0xb0,0x80,0xf8,0xe0,0x00,0x10,0xa0,0xf5,0x10,0x01,0xf5,0x10,0x02,0xf5,0x10,0x03,
	0xfe,0xc8,0xda,0xfc,0xf7,0x00,0xfe,0xff,0xf7,0x31,0xd2,0xfc,0xea,0x0b,0xc8,0xda,
	0xfc,0xf7,0x00,0xfd,0xff,0xf7,0x31,0xd6,0xfc,0xc4,0xda,0xfc,0xcc,0x44,0xfc,0xf7,
	0x00,0xfe,0xff,0xc4,0x44,0xfc,0xf4,0x7d,0x75,0xae,0x08,0xe9,0x07,0xf4,0x75,0x60,
	0xd1,0x40,0xea,0x0c,0xf4,0x7d,0x32,0x9d,0x08,0xe9,0x05,0xf4,0x75,0x94,0xd1,0x40,
	0xf2,0x7c,0xd0,0x04,0xcc,0x5b,0x80,0xd8,0x01,0xe9,0x02,0x7c,0x04,0x51,0x20,0x71,
	0x34,0xf4,0x7d,0xc1,0x85,0x08,0xe8,0x05,0xfe,0x80,0x01,0xea,0x02,0x80,0x00,0xa5,
	0xd8,0x00,0xe8,0x02,0x85,0x0c,0xc5,0xda,0xfc,0xf4,0x75,0xa0,0xd1,0x40,0x14,0xfe,
	0xf7,0x10,0xff,0xf7,0xf4,0xc9,0xa0,0xd1,0x40,0xd9,0x00,0xe8,0x22,0x21,0xf7,0x49,
	0x08,0x06,0xe9,0x05,0x85,0x02,0xf5,0x10,0x01,0xf4,0x79,0x00,0xf0,0x00,0xe9,0x05,
	0x80,0x00,0xf5,0x10,0x09,0xd9,0x06,0xe9,0x06,0x61,0x06,0xd5,0x06,0x41,0x06,0xf4,
	0xe0,0xec,0xdc,0xc7,0xf4,0xe0,0x0e,0xcc,0xc7,0x00,0x00,0x00,0x74,0x0a,0x08,0x00,
	0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

static const u32 __dvd_patchcodeQ08_size = 448;
static const u8 __dvd_patchcodeQ08[] =
{
	0xf7,0x10,0xff,0xf7,0xf4,0x74,0x25,0xd0,0x40,0xf7,0x20,0x4c,0x80,0xf4,0x74,0x39,
	0x9e,0x08,0xf7,0x20,0xd6,0xfc,0xf4,0x74,0x02,0xb3,0x08,0xf7,0x20,0xd2,0xfc,0x80,
	0x0c,0xc4,0xda,0xfc,0xfe,0xc8,0xda,0xfc,0xf5,0x00,0x01,0xe8,0x03,0xfc,0xc1,0x00,
	0xa0,0xf4,0x74,0x09,0xec,0x40,0x10,0xc8,0xda,0xfc,0xf5,0x00,0x02,0xe8,0x03,0xfc,
	0xbc,0x00,0xf4,0x74,0x02,0xed,0x40,0x80,0x02,0xf0,0x20,0xc8,0x78,0x80,0xc0,0x92,
	0x81,0xdc,0xaa,0x80,0xf5,0x30,0x00,0xf4,0x44,0xa1,0xd1,0x40,0xf8,0xaa,0x00,0x10,
	0xf4,0xd0,0x9c,0xd1,0x40,0xf0,0x01,0xdc,0xaa,0x80,0xf5,0x30,0x00,0xf7,0x48,0xaa,
	0x00,0xe9,0x07,0xf4,0xc4,0xa1,0xd1,0x40,0x10,0xfe,0xd8,0x32,0xe8,0x1d,0xf7,0x48,
	0xa8,0x00,0xe8,0x28,0xf7,0x48,0xab,0x00,0xe8,0x22,0xf7,0x48,0xe1,0x00,0xe8,0x1c,
	0xf7,0x48,0xee,0x00,0xe8,0x3d,0xd8,0x55,0xe8,0x31,0xfe,0x71,0x04,0xfd,0x22,0x00,
	0xf4,0x51,0xb0,0xd1,0x40,0xa0,0x40,0x04,0x40,0x06,0xea,0x33,0xf2,0xf9,0xf4,0xd2,
	0xb0,0xd1,0x40,0x71,0x04,0xfd,0x0a,0x00,0xf2,0x49,0xfd,0x05,0x00,0x51,0x04,0xf2,
	0x36,0xfe,0xf7,0x21,0xbc,0xff,0xf7,0x31,0xbc,0xff,0xfe,0xf5,0x30,0x01,0xfd,0x7e,
	0x00,0xea,0x0c,0xf5,0x30,0x01,0xc4,0xa6,0x81,0xf5,0x30,0x02,0xc4,0x8a,0x81,0xdc,
	0xaa,0x80,0xf8,0xe0,0x00,0x10,0xa0,0xf5,0x10,0x01,0xf5,0x10,0x02,0xf5,0x10,0x03,
	0xfe,0xc8,0xda,0xfc,0xf7,0x00,0xfe,0xff,0xf7,0x31,0xd2,0xfc,0xea,0x0b,0xc8,0xda,
	0xfc,0xf7,0x00,0xfd,0xff,0xf7,0x31,0xd6,0xfc,0xc4,0xda,0xfc,0xcc,0x44,0xfc,0xf7,
	0x00,0xfe,0xff,0xc4,0x44,0xfc,0xf4,0x7d,0x02,0xb3,0x08,0xe9,0x07,0xf4,0x75,0x60,
	0xd1,0x40,0xea,0x0c,0xf4,0x7d,0x39,0x9e,0x08,0xe9,0x05,0xf4,0x75,0x94,0xd1,0x40,
	0xf2,0x7c,0xd0,0x04,0xcc,0x5b,0x80,0xd8,0x01,0xe9,0x02,0x7c,0x04,0x51,0x20,0x71,
	0x34,0xf4,0x7d,0x7f,0x86,0x08,0xe8,0x05,0xfe,0x80,0x01,0xea,0x02,0x80,0x00,0xa5,
	0xd8,0x00,0xe8,0x02,0x85,0x0c,0xc5,0xda,0xfc,0xf4,0x75,0xa0,0xd1,0x40,0x14,0xfe,
	0xf7,0x10,0xff,0xf7,0xf4,0xc9,0xa0,0xd1,0x40,0xd9,0x00,0xe8,0x22,0x21,0xf7,0x49,
	0x08,0x06,0xe9,0x05,0x85,0x02,0xf5,0x10,0x01,0xf4,0x79,0x00,0xf0,0x00,0xe9,0x05,
	0x80,0x00,0xf5,0x10,0x09,0xd9,0x06,0xe9,0x06,0x61,0x06,0xd5,0x06,0x41,0x06,0xf4,
	0xe0,0x79,0xe1,0xc7,0xf4,0xe0,0xeb,0xcd,0xc7,0x00,0x00,0x00,0xa4,0x0a,0x08,0x00,
	0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

static vu32* const _piReg = (u32*)0xCC003000;

#if defined(HW_RVL)
	static vu32* const _diReg = (u32*)0xCD806000;
#elif defined(HW_DOL)
	static vu32* const _diReg = (u32*)0xCC006000;
#endif

static const u8 __dvd_unlockcmd$221[12] = {0xff,0x01,'M','A','T','S','H','I','T','A',0x02,0x00};
static const u8 __dvd_unlockcmd$222[12] = {0xff,0x00,'D','V','D','-','G','A','M','E',0x03,0x00};
static const u32 __dvd_errortable[] = {
	0x00000000, 0x00023a00, 0x00062800, 0x00030200,
	0x00031100, 0x00052000, 0x00052001, 0x00052100,
	0x00052400, 0x00052401, 0x00052402, 0x000B5A01,
	0x00056300, 0x00020401, 0x00020400, 0x00040800,
	0x00100007, 0x00000000
};

static void __dvd_statecheckid(void);
static void __dvd_stategettingerror(void);
static void __dvd_statecoverclosed(void);
static void __dvd_stateready(void);
static void __dvd_statemotorstopped(void);
static void __dvd_statetimeout(void);
static void __dvd_stategotoretry(void);
static void __dvd_stateerror(s32 result);
static void __dvd_statecoverclosed_cmd(dvdcmdblk *block);
static void __dvd_statebusy(dvdcmdblk *block);
static s32 __issuecommand(s32 prio,dvdcmdblk *block);

void DVD_LowReset(u32 reset_mode);
dvdcallbacklow DVD_LowSetResetCoverCallback(dvdcallbacklow cb);
s32 DVD_LowBreak(void);
dvdcallbacklow DVD_LowClearCallback(void);
s32 DVD_LowSeek(s64 offset,dvdcallbacklow cb);
s32 DVD_LowRead(void *buf,u32 len,s64 offset,dvdcallbacklow cb);
s32 DVD_LowReadDiskID(dvddiskid *diskID,dvdcallbacklow cb);
s32 DVD_LowRequestError(dvdcallbacklow cb);
s32 DVD_LowStopMotor(dvdcallbacklow cb);
s32 DVD_LowInquiry(dvddrvinfo *info,dvdcallbacklow cb);
s32 DVD_LowWaitCoverClose(dvdcallbacklow cb);
s32 DVD_LowGetCoverStatus(void);
s32 DVD_LowAudioStream(u32 subcmd,u32 len,s64 offset,dvdcallbacklow cb);
s32 DVD_LowAudioBufferConfig(s32 enable,u32 size,dvdcallbacklow cb);
s32 DVD_LowRequestAudioStatus(u32 subcmd,dvdcallbacklow cb);
s32 DVD_LowEnableExtensions(u8 enable,dvdcallbacklow cb);
s32 DVD_LowSpinMotor(u32 mode,dvdcallbacklow cb);
s32 DVD_LowSetStatus(u32 status,dvdcallbacklow cb);
s32 DVD_LowUnlockDrive(dvdcallbacklow cb);
s32 DVD_LowPatchDriveCode(dvdcallbacklow cb);
s32 DVD_LowSpinUpDrive(dvdcallbacklow cb);
s32 DVD_LowControlMotor(u32 mode,dvdcallbacklow cb);
s32 DVD_LowFuncCall(u32 address,dvdcallbacklow cb);
s32 DVD_LowReadmem(u32 address,void *buffer,dvdcallbacklow cb);
s32 DVD_LowSetGCMOffset(s64 offset,dvdcallbacklow cb);
s32 DVD_LowSetOffset(s64 offset,dvdcallbacklow cb);
s32 DVD_LowReadImm(dvdcmdbuf cmdbuf,dvdcallbacklow cb);
s32 DVD_LowWriteImm(dvdcmdbuf cmdbuf,u32 immbuf,dvdcallbacklow cb);
s32 DVD_LowReadDma(dvdcmdbuf cmdbuf,void *buf,u32 len,dvdcallbacklow cb);
s32 DVD_LowWriteDma(dvdcmdbuf cmdbuf,void *buf,u32 len,dvdcallbacklow cb);
s32 DVD_GcodeLowRead(void *buf,u32 len,u32 offset,dvdcallbacklow cb);
s32 DVD_GcodeLowWriteBuffer(void *buf,u32 len,dvdcallbacklow cb);
s32 DVD_GcodeLowWrite(u32 len,u32 offset,dvdcallbacklow cb);

extern syssramex* __SYS_LockSramEx(void);
extern u32 __SYS_UnlockSramEx(u32 write);

static u8 err2num(u32 errorcode)
{
	u32 i;

	i=0;
	while(i<18) {
		if(errorcode==__dvd_errortable[i]) return i;
		i++;
	}
	if(errorcode<0x00100000 || errorcode>0x00100008) return 29;

	return 17;
}

static u8 convert(u32 errorcode)
{
	u8 err,err_num;

	if((errorcode-0x01230000)==0x4567) return 255;
	else if((errorcode-0x01230000)==0x4568) return 254;

	err = _SHIFTR(errorcode,24,8);
	err_num = err2num((errorcode&0x00ffffff));
	if(err>0x06) err = 0x06;

	return err_num+(err*30);
}

static void __dvd_clearwaitingqueue(void)
{
	u32 i;

	for(i=0;i<4;i++)
		__lwp_queue_init_empty(&__dvd_waitingqueue[i]);
}

static s32 __dvd_checkwaitingqueue(void)
{
	u32 i;
	u32 level;

	_CPU_ISR_Disable(level);
	for(i=0;i<4;i++) {
		if(!__lwp_queue_isempty(&__dvd_waitingqueue[i])) break;
	}
	_CPU_ISR_Restore(level);
	return (i<4);
}

static s32 __dvd_dequeuewaitingqueue(dvdcmdblk *block)
{
	u32 level;

	_CPU_ISR_Disable(level);
	__lwp_queue_extractI(&block->node);
	_CPU_ISR_Restore(level);
	return 1;
}

static s32 __dvd_pushwaitingqueue(s32 prio,dvdcmdblk *block)
{
	u32 level;
#ifdef _DVD_DEBUG
	printf("__dvd_pushwaitingqueue(%d,%p,%p)\n",prio,block,block->cb);
#endif
	_CPU_ISR_Disable(level);
	__lwp_queue_appendI(&__dvd_waitingqueue[prio],&block->node);
	_CPU_ISR_Restore(level);
	return 1;
}

static dvdcmdblk* __dvd_popwaitingqueueprio(s32 prio)
{
	u32 level;
	dvdcmdblk *ret = NULL;
#ifdef _DVD_DEBUG
	printf("__dvd_popwaitingqueueprio(%d)\n",prio);
#endif
	_CPU_ISR_Disable(level);
	ret = (dvdcmdblk*)__lwp_queue_firstnodeI(&__dvd_waitingqueue[prio]);
	_CPU_ISR_Restore(level);
#ifdef _DVD_DEBUG
	printf("__dvd_popwaitingqueueprio(%p,%p)\n",ret,ret->cb);
#endif
	return ret;
}

static dvdcmdblk* __dvd_popwaitingqueue(void)
{
	u32 i,level;
	dvdcmdblk *ret = NULL;
#ifdef _DVD_DEBUG
	printf("__dvd_popwaitingqueue()\n");
#endif
	_CPU_ISR_Disable(level);
	for(i=0;i<4;i++) {
		if(!__lwp_queue_isempty(&__dvd_waitingqueue[i])) {
			ret = __dvd_popwaitingqueueprio(i);
			_CPU_ISR_Restore(level);
			return ret;
		}
	}
	_CPU_ISR_Restore(level);
	return NULL;
}

static void __dvd_timeouthandler(syswd_t alarm,void *cbarg)
{
	dvdcallbacklow cb;

	__MaskIrq(IRQMASK(IRQ_PI_DI));
	cb = __dvd_callback;
	__dvd_callback = NULL;
	if(cb) cb(0x10);
}

static void __dvd_storeerror(u32 errorcode)
{
	u8 err;
	syssramex *ptr;

	err = convert(errorcode);
	ptr = __SYS_LockSramEx();
	ptr->dvderr_code = err;
	__SYS_UnlockSramEx(1);
}

static u32 __dvd_categorizeerror(u32 errorcode)
{
#ifdef _DVD_DEBUG
	printf("__dvd_categorizeerror(%08x)\n",errorcode);
#endif
	if((errorcode-0x20000)==0x0400) {
		__dvd_lasterror = errorcode;
		return 1;
	}

	if(DVD_ERROR(errorcode)==DVD_ERROR_MEDIUM_CHANGED
		|| DVD_ERROR(errorcode)==DVD_ERROR_MEDIUM_NOT_PRESENT
		|| DVD_ERROR(errorcode)==DVD_ERROR_MEDIUM_CHANGE_REQ
		|| (DVD_ERROR(errorcode)-0x40000)==0x3100) return 0;

	__dvd_internalretries++;
	if(__dvd_internalretries==2) {
		if(__dvd_lasterror==DVD_ERROR(errorcode)) {
			__dvd_lasterror = DVD_ERROR(errorcode);
			return 1;
		}
		__dvd_lasterror = DVD_ERROR(errorcode);
		return 2;
	}

	__dvd_lasterror = DVD_ERROR(errorcode);
	if(DVD_ERROR(errorcode)!=DVD_ERROR_UNRECOVERABLE_READ) {
		if(__dvd_executing->cmd!=0x0005) return 3;
	}
	return 2;
}

static void __SetupTimeoutAlarm(const struct timespec *tp)
{
	SYS_SetAlarm(__dvd_timeoutalarm,tp,__dvd_timeouthandler,NULL);
}

static void __Read(void *buffer,u32 len,s64 offset,dvdcallbacklow cb)
{
	struct timespec tb;
#ifdef _DVD_DEBUG
	printf("__Read(%p,%d,%d)\n",buffer,len,offset);
#endif
	__dvd_callback = cb;
	__dvd_stopnextint = 0;
	__dvd_lastcmdwasread = 1;

	_diReg[2] = DVD_READ;
	_diReg[3] = (u32)(offset>>2);
	_diReg[4] = len;
	_diReg[5] = (u32)buffer;
	_diReg[6] = len;

	__dvd_lastlen = len;

	_diReg[7] = (DVD_DI_DMA|DVD_DI_START);

	if(len>0x00a00000) {
		tb.tv_sec = 20;
		tb.tv_nsec = 0;
		__SetupTimeoutAlarm(&tb);
	} else {
		tb.tv_sec = 10;
		tb.tv_nsec = 0;
		__SetupTimeoutAlarm(&tb);
	}
}

static void __DoRead(void *buffer,u32 len,s64 offset,dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("__DoRead(%p,%d,%d)\n",buffer,len,offset);
#endif
	__dvd_nextcmdnum = 0;
	__dvd_cmdlist[0].cmd = -1;
	__Read(buffer,len,offset,cb);
}

static u32 __ProcessNextCmd(void)
{
	u32 cmd_num;
#ifdef _DVD_DEBUG
	printf("__ProcessNextCmd(%d)\n",__dvd_nextcmdnum);
#endif
	cmd_num = __dvd_nextcmdnum;
	if(__dvd_cmdlist[cmd_num].cmd==0x0001) {
		__dvd_nextcmdnum++;
		__Read(__dvd_cmdlist[cmd_num].buf,__dvd_cmdlist[cmd_num].len,__dvd_cmdlist[cmd_num].offset,__dvd_cmdlist[cmd_num].cb);
		return 1;
	}

	if(__dvd_cmdlist[cmd_num].cmd==0x0002) {
		__dvd_nextcmdnum++;
		DVD_LowSeek(__dvd_cmdlist[cmd_num].offset,__dvd_cmdlist[cmd_num].cb);
		return 1;
	}
	return 0;
}

static void __DVDLowWATypeSet(u32 workaround,u32 workaroundseek)
{
	u32 level;

	_CPU_ISR_Disable(level);
	__dvd_workaround = workaround;
	__dvd_workaroundseek = workaroundseek;
	_CPU_ISR_Restore(level);
}

static void __DVDInitWA(void)
{
	__dvd_nextcmdnum = 0;
	__DVDLowWATypeSet(0,0);
}

static s32 __dvd_checkcancel(u32 cancelpt)
{
	dvdcmdblk *block;

	if(__dvd_canceling) {
		__dvd_resumefromhere = cancelpt;
		__dvd_canceling = 0;

		block = __dvd_executing;
		__dvd_executing = &__dvd_dummycmdblk;

		block->state = DVD_STATE_CANCELED;
		if(block->cb) block->cb(DVD_ERROR_CANCELED,block);
		if(__dvd_cancelcallback) __dvd_cancelcallback(DVD_ERROR_OK,block);

		__dvd_stateready();
		return 1;
	}
	return 0;
}

static void __dvd_stateretrycb(s32 result)
{
#ifdef _DVD_DEBUG
	printf("__dvd_stateretrycb(%08x)\n",result);
#endif
	if(result==0x0010) {
		__dvd_executing->state = DVD_STATE_FATAL_ERROR;
		__dvd_statetimeout();
		return;
	}

	if(result&0x0002) __dvd_stateerror(0x01234567);
	if(result==0x0001) {
		__dvd_statebusy(__dvd_executing);
		return;
	}
}

static void __dvd_unrecoverederrorretrycb(s32 result)
{
	u32 val;

	if(result==0x0010) {
		__dvd_executing->state = DVD_STATE_FATAL_ERROR;
		__dvd_statetimeout();
		return;
	}

	__dvd_executing->state = DVD_STATE_FATAL_ERROR;
	if(result&0x0002) __dvd_stateerror(0x01234567);
	else {
		val = _diReg[8];
		__dvd_stateerror(val);
	}
}

static void __dvd_unrecoverederrorcb(s32 result)
{
	if(result==0x0010) {
		__dvd_executing->state = DVD_STATE_FATAL_ERROR;
		__dvd_statetimeout();
		return;
	}
	if(result&0x0001) {
		__dvd_stategotoretry();
		return;
	}
	if(!(result==0x0002)) DVD_LowRequestError(__dvd_unrecoverederrorretrycb);
}

static void __dvd_stateerrorcb(s32 result)
{
	dvdcmdblk *block;
#ifdef _DVD_DEBUG
	printf("__dvd_stateerrorcb(%d)\n",result);
#endif
	if(result==0x0010) {
		__dvd_executing->state = DVD_STATE_FATAL_ERROR;
		__dvd_statetimeout();
		return;
	}

	__dvd_fatalerror = 1;
	block = __dvd_executing;
	__dvd_executing = &__dvd_dummycmdblk;
	if(block->cb) block->cb(DVD_ERROR_FATAL,block);
	if(__dvd_canceling) {
		__dvd_canceling = 0;
		if(__dvd_cancelcallback) __dvd_cancelcallback(DVD_ERROR_OK,block);
	}
	__dvd_stateready();
}

static void __dvd_stategettingerrorcb(s32 result)
{
	s32 ret;
	u32 val,cnclpt;
#ifdef _DVD_DEBUG
	printf("__dvd_stategettingerrorcb(%d)\n",result);
#endif
	if(result==0x0010) {
		__dvd_executing->state = DVD_STATE_FATAL_ERROR;
		__dvd_statetimeout();
		return;
	}
	if(result&0x0002) {
		__dvd_executing->state = DVD_STATE_FATAL_ERROR;
		__dvd_stateerror(0x01234567);
		return;
	}
	if(result==0x0001) {
		val = _diReg[8];
		ret = __dvd_categorizeerror(val);
#ifdef _DVD_DEBUG
		printf("__dvd_stategettingerrorcb(status = %02x, err = %08x, category = %d)\n",DVD_STATUS(val),DVD_ERROR(val),ret);
#endif
		if(ret==1) {
			__dvd_executing->state = DVD_STATE_FATAL_ERROR;
			__dvd_stateerror(val);
			return;
		} else if(ret==2 || ret==3) cnclpt = 0;
		else {
			if(DVD_STATUS(val)==DVD_STATUS_COVER_OPENED) cnclpt = 4;
			else if(DVD_STATUS(val)==DVD_STATUS_DISK_CHANGE) cnclpt = 6;
			else if(DVD_STATUS(val)==DVD_STATUS_NO_DISK) cnclpt = 3;
			else cnclpt = 5;
		}
		if(__dvd_checkcancel(cnclpt)) return;

		if(ret==2) {
			if(dvd_may_retry(val)) {
				// disable the extensions if they're enabled and we were trying to read the disc ID
				if(__dvd_executing->cmd==0x0005) {
					__dvd_lasterror = 0;
					__dvd_extensionsenabled = FALSE;
					DVD_LowSpinUpDrive(__dvd_stateretrycb);
					return;
				}
				__dvd_statebusy(__dvd_executing);
			} else {
				__dvd_storeerror(val);
				__dvd_stategotoretry();
			}
			return;
		} else if(ret==3) {
			if(DVD_ERROR(val)==DVD_ERROR_UNRECOVERABLE_READ) {
				DVD_LowSeek(__dvd_executing->offset,__dvd_unrecoverederrorcb);
				return;
			} else {
				__dvd_laststate(__dvd_executing);
				return;
			}
		} else if(DVD_STATUS(val)==DVD_STATUS_COVER_OPENED) {
			__dvd_executing->state = DVD_STATE_COVER_OPEN;
			__dvd_statemotorstopped();
			return;
		} else if(DVD_STATUS(val)==DVD_STATUS_DISK_CHANGE) {
			__dvd_executing->state = DVD_STATE_COVER_CLOSED;
			__dvd_statecoverclosed();
			return;
		} else if(DVD_STATUS(val)==DVD_STATUS_NO_DISK) {
			__dvd_executing->state = DVD_STATE_NO_DISK;
			__dvd_statemotorstopped();
			return;
		}
		__dvd_executing->state = DVD_STATE_FATAL_ERROR;
		__dvd_stateerror(0x01234567);
		return;
	}
}

static void __dvd_statebusycb(s32 result)
{
	u32 val;
	dvdcmdblk *block;
#ifdef _DVD_DEBUG
	printf("__dvd_statebusycb(%04x)\n",result);
#endif
	if(result==0x0010) {
		__dvd_executing->state = DVD_STATE_FATAL_ERROR;
		__dvd_statetimeout();
		return;
	}
	if(__dvd_currcmd==0x0003 || __dvd_currcmd==0x000f) {
		if(result&0x0002) {
			__dvd_executing->state = DVD_STATE_FATAL_ERROR;
			__dvd_stateerror(0x01234567);
			return;
		}
		if(result==0x0001) {
			__dvd_internalretries = 0;
			if(__dvd_currcmd==0x000f) __dvd_resetrequired = 1;
			if(__dvd_checkcancel(7)) return;

			__dvd_executing->state = DVD_STATE_MOTOR_STOPPED;
			__dvd_statemotorstopped();
			return;
		}
	}
	if(result&0x0004) {

	}
	if(__dvd_currcmd==0x0001 || __dvd_currcmd==0x0004
		|| __dvd_currcmd==0x0005 || __dvd_currcmd==0x000e
		|| __dvd_currcmd==0x0016 || __dvd_currcmd==0x0017
		|| __dvd_currcmd==0x0018) {
		__dvd_executing->txdsize += (__dvd_executing->currtxsize-_diReg[6]);
	}
	if(result&0x0008) {
		__dvd_canceling = 0;
		block = __dvd_executing;
		__dvd_executing = &__dvd_dummycmdblk;
		block->state = DVD_STATE_CANCELED;
		if(block->cb) block->cb(DVD_ERROR_CANCELED,block);
		if(__dvd_cancelcallback) __dvd_cancelcallback(DVD_ERROR_OK,block);
		__dvd_stateready();
		return;
	}
	if(result&0x0001) {
		__dvd_internalretries = 0;
		if(__dvd_currcmd==0x0010) {
			block = __dvd_executing;
			__dvd_executing = &__dvd_dummycmdblk;
			block->state = DVD_STATE_END;
			if(block->cb) block->cb(DVD_ERROR_OK,block);
			__dvd_stateready();
			return;
		}
		if(__dvd_checkcancel(0)) return;

		if(__dvd_currcmd==0x0014 || __dvd_currcmd==0x0015) {
			if(__dvd_currcmd==0x0014) {
				val = _diReg[8];
				__stswx(__dvd_executing->buf,__dvd_executing->currtxsize,val);
			}
			__dvd_executing->txdsize += __dvd_executing->currtxsize;

			block = __dvd_executing;
			__dvd_executing = &__dvd_dummycmdblk;
			block->state = DVD_STATE_END;
			if(block->cb) block->cb(block->txdsize,block);
			__dvd_stateready();
			return;
		}
		if(__dvd_currcmd==0x0016 || __dvd_currcmd==0x0017) {
			block = __dvd_executing;
			__dvd_executing = &__dvd_dummycmdblk;
			block->state = DVD_STATE_END;
			if(block->cb) block->cb(block->txdsize,block);
			__dvd_stateready();
			return;
		}
		if(__dvd_currcmd==0x0001 || __dvd_currcmd==0x0004
			|| __dvd_currcmd==0x0005 || __dvd_currcmd==0x000e
			|| __dvd_currcmd==0x0018) {
#ifdef _DVD_DEBUG
			printf("__dvd_statebusycb(%p,%p)\n",__dvd_executing,__dvd_executing->cb);
#endif
			if(__dvd_executing->txdsize!=__dvd_executing->len) {
				__dvd_statebusy(__dvd_executing);
				return;
			}
			block = __dvd_executing;
			__dvd_executing = &__dvd_dummycmdblk;
			block->state = DVD_STATE_END;
			if(block->cb) block->cb(block->txdsize,block);
			__dvd_stateready();
			return;
		}
		if(__dvd_currcmd==0x0019) {
			if(_diReg[7]&DVD_DI_MODE) {
				DVD_GcodeLowWrite(__dvd_executing->currtxsize,__dvd_executing->offset+__dvd_executing->txdsize,__dvd_statebusycb);
				return;
			}
			if(_diReg[8]) {
				block = __dvd_executing;
				__dvd_executing = &__dvd_dummycmdblk;
				block->state = DVD_STATE_FATAL_ERROR;
				if(block->cb) block->cb(DVD_ERROR_FATAL,block);
				__dvd_stateready();
				return;
			}
			__dvd_executing->txdsize += __dvd_executing->currtxsize;
			if(__dvd_executing->txdsize!=__dvd_executing->len) {
				__dvd_statebusy(__dvd_executing);
				return;
			}
			block = __dvd_executing;
			__dvd_executing = &__dvd_dummycmdblk;
			block->state = DVD_STATE_END;
			if(block->cb) block->cb(block->txdsize,block);
			__dvd_stateready();
			return;
		}
		if(__dvd_currcmd==0x0009 || __dvd_currcmd==0x000a
			|| __dvd_currcmd==0x000b || __dvd_currcmd==0x000c) {

			val = _diReg[8];
			if(__dvd_currcmd==0x000a || __dvd_currcmd==0x000b) val <<= 2;
			__dvd_executing->txdsize = val;

			block = __dvd_executing;
			__dvd_executing = &__dvd_dummycmdblk;
			block->state = DVD_STATE_END;
			if(block->cb) block->cb(block->txdsize,block);
			__dvd_stateready();
			return;
		}
		if(__dvd_currcmd==0x0006) {
			if(!__dvd_executing->currtxsize) {
				if(_diReg[8]&0x0001) {
					block = __dvd_executing;
					__dvd_executing = &__dvd_dummycmdblk;
					block->state = DVD_STATE_IGNORED;
					if(block->cb) block->cb(DVD_ERROR_IGNORED,block);
					__dvd_stateready();
					return;
				}
				__dvd_autofinishing = 0;
				__dvd_executing->currtxsize = 1;
				DVD_LowAudioStream(0,__dvd_executing->len,__dvd_executing->offset,__dvd_statebusycb);
				return;
			}

			block = __dvd_executing;
			__dvd_executing = &__dvd_dummycmdblk;
			block->state = DVD_STATE_END;
			if(block->cb) block->cb(DVD_ERROR_OK,block);
			__dvd_stateready();
			return;
		}
		if(__dvd_currcmd==0x0011) {
			if(__dvd_drivestate&DVD_CHIPPRESENT) {
				block = __dvd_executing;
				__dvd_executing = &__dvd_dummycmdblk;
				block->state = DVD_STATE_END;
				if(block->cb) block->cb(DVD_DISKIDSIZE,block);
				__dvd_stateready();
				return;
			}
		}

		block = __dvd_executing;
		__dvd_executing = &__dvd_dummycmdblk;
		block->state = DVD_STATE_END;
		if(block->cb) block->cb(DVD_ERROR_OK,block);
		__dvd_stateready();
		return;
	}
	if(result==0x0002) {
#ifdef _DVD_DEBUG
		printf("__dvd_statebusycb(%d,%d)\n",__dvd_executing->txdsize,__dvd_executing->len);
#endif
		if(__dvd_currcmd==0x000e) {
			__dvd_executing->state = DVD_STATE_FATAL_ERROR;
			__dvd_stateerror(0x01234567);
			return;
		}
		if(__dvd_currcmd==0x0014 || __dvd_currcmd==0x0015
			|| __dvd_currcmd==0x0016 || __dvd_currcmd==0x0017) {
			if(__dvd_checkcancel(0)) return;

			block = __dvd_executing;
			__dvd_executing = &__dvd_dummycmdblk;
			block->state = DVD_STATE_FATAL_ERROR;
			if(block->cb) block->cb(DVD_ERROR_FATAL,block);
			__dvd_stateready();
			return;
		}
		if((__dvd_currcmd==0x0001 || __dvd_currcmd==0x0004
			|| __dvd_currcmd==0x0005 || __dvd_currcmd==0x000e
			|| __dvd_currcmd==0x0018)
			&& __dvd_executing->txdsize==__dvd_executing->len) {
				if(__dvd_checkcancel(0)) return;

				block = __dvd_executing;
				__dvd_executing = &__dvd_dummycmdblk;
				block->state = DVD_STATE_END;
				if(block->cb) block->cb(block->txdsize,block);
				__dvd_stateready();
				return;
		}
	}
	__dvd_stategettingerror();
}

static void __dvd_synccallback(s32 result,dvdcmdblk *block)
{
	LWP_ThreadBroadcast(__dvd_wait_queue);
}

static void __dvd_statemotorstoppedcb(s32 result)
{
#ifdef _DVD_DEBUG
	printf("__dvd_statemotorstoppedcb(%08x)\n",result);
#endif
	_diReg[1] = 0;
	__dvd_executing->state = DVD_STATE_COVER_CLOSED;
	__dvd_statecoverclosed();
}

static void __dvd_statecoverclosedcb(s32 result)
{
#ifdef _DVD_DEBUG
	printf("__dvd_statecoverclosedcb(%08x)\n",result);
#endif
	if(result==0x0010) {
		__dvd_executing->state = DVD_STATE_FATAL_ERROR;
		__dvd_statetimeout();
		return;
	}

	if(!(result&0x0006)) {
		__dvd_internalretries = 0;
		__dvd_statecheckid();
		return;
	}

	if(result==0x0002) __dvd_stategettingerror();
}

static void __dvd_statecheckid1cb(s32 result)
{
#ifdef _DVD_DEBUG
	printf("__dvd_statecheckid1cb(%08x)\n",result);
#endif
	__dvd_ready = 1;
	if(memcmp(__dvd_diskID,&__dvd_tmpid0,DVD_DISKIDSIZE)) memcpy(__dvd_diskID,&__dvd_tmpid0,DVD_DISKIDSIZE);
	LWP_ThreadBroadcast(__dvd_wait_queue);
}

static void __dvd_stategotoretrycb(s32 result)
{
	if(result==0x0010) {
		__dvd_executing->state = DVD_STATE_FATAL_ERROR;
		__dvd_statetimeout();
		return;
	}
	if(result&0x0002) {
		__dvd_executing->state = DVD_STATE_FATAL_ERROR;
		__dvd_stateerror(0x01234567);
		return;
	}
	if(result==0x0001) {
		__dvd_internalretries = 0;
		if(__dvd_currcmd==0x0004 || __dvd_currcmd==0x0005
			|| __dvd_currcmd==0x000d || __dvd_currcmd==0x000f) {
			__dvd_resetrequired = 1;
			if(__dvd_checkcancel(2)) return;

			__dvd_executing->state = DVD_STATE_RETRY;
			__dvd_statemotorstopped();
		}
	}
}

static void __dvd_getstatuscb(s32 result)
{
	u32 val,*pn_data;
	dvdcallbacklow cb;
#ifdef _DVD_DEBUG
	printf("__dvd_getstatuscb(%d)\n",result);
#endif
	if(result==0x0010) {
		__dvd_statetimeout();
		return;
	}
	if(result==0x0001) {
		val = _diReg[8];

		pn_data = __dvd_usrdata;
		__dvd_usrdata = NULL;
		if(pn_data) pn_data[0] = val;

		cb = __dvd_finalstatuscb;
		__dvd_finalstatuscb = NULL;
		if(cb) cb(result);
		return;
	}
	__dvd_stategettingerror();
}

static void __dvd_readmemcb(s32 result)
{
	u32 val,*pn_data;
	dvdcallbacklow cb;
#ifdef _DVD_DEBUG
	printf("__dvd_readmemcb(%d)\n",result);
#endif
	if(result==0x0010) {
		__dvd_statetimeout();
		return;
	}
	if(result==0x0001) {
		val = _diReg[8];

		pn_data = __dvd_usrdata;
		__dvd_usrdata = NULL;
		if(pn_data) pn_data[0] = val;

		cb = __dvd_finalreadmemcb;
		__dvd_finalreadmemcb = NULL;
		if(cb) cb(result);
		return;
	}
	__dvd_stategettingerror();
}

static void __dvd_cntrldrivecb(s32 result)
{
#ifdef _DVD_DEBUG
	printf("__dvd_cntrldrivecb(%d)\n",result);
#endif
	if(result==0x0010) {
		__dvd_statetimeout();
		return;
	}
	if(result==0x0001) {
		DVD_LowSpinMotor(__dvd_motorcntrl,__dvd_finalsudcb);
		return;
	}
	__dvd_stategettingerror();
}

static void __dvd_setgcmoffsetcb(s32 result)
{
	s64 *pn_data,offset;
#ifdef _DVD_DEBUG
	printf("__dvd_setgcmoffsetcb(%d)\n",result);
#endif
	if(result==0x0010) {
		__dvd_statetimeout();
		return;
	}
	if(result==0x0001) {
		pn_data = (s64*)__dvd_usrdata;
		__dvd_usrdata = NULL;

		offset = 0;
		if(pn_data) offset = *pn_data;
		DVD_LowSetOffset(offset,__dvd_finaloffsetcb);
		return;
	}
	__dvd_stategettingerror();
}

static void __dvd_handlespinupcb(s32 result)
{
	static u32 step = 0;
#ifdef _DVD_DEBUG
	printf("__dvd_handlespinupcb(%d,%d)\n",result,step);
#endif
	if(result==0x0010) {
		__dvd_statetimeout();
		return;
	}
	if(result==0x0001) {
		if(step==0x0000) {
			step++;
			_diReg[1] = _diReg[1];
			DVD_LowEnableExtensions(__dvd_extensionsenabled,__dvd_handlespinupcb);
			return;
		}
		if(step==0x0001) {
			step++;
			_diReg[1] = _diReg[1];
			DVD_LowSpinMotor((DVD_SPINMOTOR_ACCEPT|DVD_SPINMOTOR_UP),__dvd_handlespinupcb);
			return;
		}
		if(step==0x0002) {
			step = 0;
			if(!__dvd_lasterror) {
				_diReg[1] = _diReg[1];
				DVD_LowSetStatus((_SHIFTL((DVD_STATUS_DISK_ID_NOT_READ+1),16,8)|0x00000300),__dvd_finalsudcb);
				return;
			}
			__dvd_finalsudcb(result);
			return;
		}
	}

	step = 0;
	__dvd_stategettingerror();
}

static void __dvd_fwpatchcb(s32 result)
{
	static u32 step = 0;
#ifdef _DVD_DEBUG
	printf("__dvd_fwpatchcb(%d,%d)\n",result,step);
#endif
	if(result==0x0010) {
		__dvd_statetimeout();
		return;
	}
	if(result==0x0001) {
		if(step==0x0000) {
			step++;
			_diReg[1] = _diReg[1];
			DVD_LowUnlockDrive(__dvd_fwpatchcb);
			return;
		}
		if(step==0x0001) {
			step = 0;
			DVD_LowPatchDriveCode(__dvd_finalpatchcb);
			return;
		}
	}

	step = 0;
	__dvd_stategettingerror();
}

static void __dvd_checkaddonscb(s32 result)
{
	u32 txdsize;
	dvdcallbacklow cb;
#ifdef _DVD_DEBUG
	printf("__dvd_checkaddonscb(%d)\n",result);
#endif
	__dvd_drivechecked = 1;
	txdsize = (DVD_DISKIDSIZE-_diReg[6]);
	if(result&0x0001) {
		// if the read was successful but was interrupted by a break we issue the read again.
		if(txdsize!=DVD_DISKIDSIZE) {
			_diReg[1] = _diReg[1];
			DCInvalidateRange(&__dvd_tmpid0,DVD_DISKIDSIZE);
			DVD_LowReadDiskID(&__dvd_tmpid0,__dvd_checkaddonscb);
			return;
		}
		__dvd_drivestate |= DVD_CHIPPRESENT;
	}

	cb = __dvd_finaladdoncb;
	__dvd_finaladdoncb = NULL;
	if(cb) cb(0x01);
}

static void __dvd_checkaddons(dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("__dvd_checkaddons()\n");
#endif
	__dvd_finaladdoncb = cb;

	// try to read disc ID.
	_diReg[1] = _diReg[1];
	DCInvalidateRange(&__dvd_tmpid0,DVD_DISKIDSIZE);
	DVD_LowReadDiskID(&__dvd_tmpid0,__dvd_checkaddonscb);
	return;
}

static void __dvd_fwpatchmem(dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("__dvd_fwpatchmem()\n");
#endif
	__dvd_finalpatchcb = cb;

	_diReg[1] = _diReg[1];
	DCInvalidateRange(&__dvd_driveinfo,DVD_DRVINFSIZE);
	DVD_LowInquiry(&__dvd_driveinfo,__dvd_fwpatchcb);
	return;
}

static void __dvd_handlespinup(void)
{
#ifdef _DVD_DEBUG
	printf("__dvd_handlespinup()\n");
#endif
	_diReg[1] = _diReg[1];
	DVD_LowUnlockDrive(__dvd_handlespinupcb);
	return;
}

static void __dvd_spinupdrivecb(s32 result)
{
#ifdef _DVD_DEBUG
	printf("__dvd_spinupdrivecb(%d,%02x,%02x)\n",result,__dvd_resetoccured,__dvd_drivestate);
#endif
	if(result==0x0010) {
		__dvd_statetimeout();
		return;
	}
	if(result==0x0001) {
		if(!__dvd_drivechecked) {
			__dvd_checkaddons(__dvd_spinupdrivecb);
			return;
		}
		if(!(__dvd_drivestate&DVD_CHIPPRESENT)) {
			if(!(__dvd_drivestate&DVD_INTEROPER)) {
				if(!(__dvd_drivestate&DVD_DRIVERESET)) {
					DVD_Reset(DVD_RESETHARD);
					udelay(1150*1000);
				}
				__dvd_fwpatchmem(__dvd_spinupdrivecb);
				return;
			}
			__dvd_handlespinup();
			return;
		}

		__dvd_finalsudcb(result);
		return;
	}
	__dvd_stategettingerror();
}

static void __dvd_statecoverclosed_spinupcb(s32 result)
{
	DCInvalidateRange(&__dvd_tmpid0,DVD_DISKIDSIZE);
	__dvd_laststate = __dvd_statecoverclosed_cmd;
	__dvd_statecoverclosed_cmd(__dvd_executing);
}

static void __DVDInterruptHandler(u32 nIrq,frame_context *pCtx)
{
	s64 now;
	u32 status,ir,irm,irmm,diff;
	dvdcallbacklow cb;

	irmm = 0;
	if(__dvd_lastcmdwasread) {
		__dvd_cmd_prev.buf = __dvd_cmd_curr.buf;
		__dvd_cmd_prev.len = __dvd_cmd_curr.len;
		__dvd_cmd_prev.offset = __dvd_cmd_curr.offset;
		if(__dvd_stopnextint) irmm |= 0x0008;
	}
	__dvd_lastcmdwasread = 0;
	__dvd_stopnextint = 0;

	status = _diReg[0];
	irm = (status&(DVD_DE_MSK|DVD_TC_MSK|DVD_BRK_MSK));
	ir = ((status&(DVD_DE_INT|DVD_TC_INT|DVD_BRK_INT))&(irm<<1));
#ifdef _DVD_DEBUG
	printf("__DVDInterruptHandler(status: %08x,irm: %08x,ir: %08x (%d))\n",status,irm,ir,__dvd_resetoccured);
#endif
	if(ir&DVD_BRK_INT) irmm |= 0x0008;
	if(ir&DVD_TC_INT) irmm |= 0x0001;
	if(ir&DVD_DE_INT) irmm |= 0x0002;

	if(irmm) {
		__dvd_resetoccured = 0;
		SYS_CancelAlarm(__dvd_timeoutalarm);
	}
	_diReg[0] = (ir|irm);

	now = __SYS_GetSystemTime();
	diff = diff_msec(__dvd_lastresetend,now);
	if(__dvd_resetoccured && diff<200) {
		status = _diReg[1];
		irm = status&DVD_CVR_MSK;
		ir = (status&DVD_CVR_INT)&(irm<<1);
		if(ir&0x0004) {
			cb = __dvd_resetcovercb;
			__dvd_resetcovercb = NULL;
			if(cb) cb(0x0004);
		}
		_diReg[1] = (ir|irm);
	} else {
		if(__dvd_waitcoverclose) {
			status = _diReg[1];
			irm = status&DVD_CVR_MSK;
			ir = (status&DVD_CVR_INT)&(irm<<1);
			if(ir&DVD_CVR_INT) irmm |= 0x0004;
			_diReg[1] = (ir|irm);
			__dvd_waitcoverclose = 0;
		} else
			_diReg[1] = 0;
	}

	if(irmm&0x0008) {
		if(!__dvd_breaking) irmm &= ~0x0008;
	}

	if(irmm&0x0001) {
		if(__ProcessNextCmd()) return;
	} else {
		__dvd_cmdlist[0].cmd = -1;
		__dvd_nextcmdnum = 0;
	}

	if(irmm) {
		cb = __dvd_callback;
		__dvd_callback = NULL;
		if(cb) cb(irmm);
		__dvd_breaking = 0;
	}
}

static void __dvd_patchdrivecb(s32 result)
{
	u32 cnt = 0;
	static u32 cmd_buf[3];
	static u32 stage = 0;
	static u32 nPos = 0;
	static u32 drv_address = 0x0040d000;
	const u32 chunk_size = 3*sizeof(u32);

#ifdef _DVD_DEBUG
	printf("__DVDPatchDriveCode()\n");
#endif
	if(__dvdpatchcode==NULL || __dvdpatchcode_size<=0) return;

	while(stage!=0x0003) {
		__dvd_callback = __dvd_patchdrivecb;
		if(stage==0x0000) {
#ifdef _DVD_DEBUG
			printf("__DVDPatchDriveCode(0x%08x,%02x,%d)\n",drv_address,stage,nPos);
#endif
			for(cnt=0;cnt<chunk_size && nPos<__dvdpatchcode_size;cnt++,nPos++) ((u8*)cmd_buf)[cnt] = __dvdpatchcode[nPos];

			if(nPos>=__dvdpatchcode_size) stage = 0x0002;
			else stage = 0x0001;

			_diReg[1] = _diReg[1];
			_diReg[2] = DVD_FWWRITEMEM;
			_diReg[3] = drv_address;
			_diReg[4] = _SHIFTL(cnt,16,16);
			_diReg[7] = (DVD_DI_DMA|DVD_DI_START);

			drv_address += cnt;
			return;
		}

		if(stage>=0x0001) {
#ifdef _DVD_DEBUG
			printf("__DVDPatchDriveCode(%02x)\n",stage);
#endif
			if(stage>0x0001) stage = 0x0003;
			else stage = 0;

			_diReg[1] = _diReg[1];
			_diReg[2] = cmd_buf[0];
			_diReg[3] = cmd_buf[1];
			_diReg[4] = cmd_buf[2];
			_diReg[7] = DVD_DI_START;
			return;
		}
	}
	__dvd_callback = NULL;
	__dvdpatchcode = NULL;
	__dvd_drivestate |= DVD_INTEROPER;
	DVD_LowFuncCall(0x0040d000,__dvd_finalpatchcb);
}


static void __dvd_unlockdrivecb(s32 result)
{
	u32 i;
#ifdef _DVD_DEBUG
	printf("__DVDUnlockDrive(%d)\n",result);
#endif
	__dvd_callback = __dvd_finalunlockcb;
	_diReg[1] = _diReg[1];
	for(i=0;i<3;i++) _diReg[2+i] = ((const u32*)__dvd_unlockcmd$222)[i];
	_diReg[7] = DVD_DI_START;
}

void __DVDPrepareResetAsync(dvdcbcallback cb)
{
	u32 level;

	_CPU_ISR_Disable(level);
	__dvd_clearwaitingqueue();
	if(__dvd_canceling) __dvd_cancelcallback = cb;
	else {
		if(__dvd_executing) __dvd_executing->cb = NULL;
		DVD_CancelAllAsync(cb);
	}
	_CPU_ISR_Restore(level);
}

static void __dvd_statebusy(dvdcmdblk *block)
{
	u32 len;
#ifdef _DVD_DEBUG
	printf("__dvd_statebusy(%p)\n",block);
#endif
	__dvd_laststate = __dvd_statebusy;

	switch(block->cmd) {
		case 1:					//Read(Sector)
		case 4:
			if(!block->len) {
				block = __dvd_executing;
				__dvd_executing = &__dvd_dummycmdblk;
				block->state = DVD_STATE_END;
				if(block->cb) block->cb(DVD_ERROR_OK,block);
				__dvd_stateready();
				return;
			}
			_diReg[1] = _diReg[1];
			len = block->len-block->txdsize;
			if(len<0x80000) block->currtxsize = len;
			else block->currtxsize = 0x80000;
			DVD_LowRead(block->buf+block->txdsize,block->currtxsize,block->offset+block->txdsize,__dvd_statebusycb);
			return;
		case 2:					//Seek(Sector)
			_diReg[1] = _diReg[1];
			DVD_LowSeek(block->offset,__dvd_statebusycb);
			return;
		case 3:
		case 15:
			DVD_LowStopMotor(__dvd_statebusycb);
			return;
		case 5:					//ReadDiskID
			_diReg[1] = _diReg[1];
			block->currtxsize = DVD_DISKIDSIZE;
			DVD_LowReadDiskID(block->buf,__dvd_statebusycb);
			return;
		case 6:
			_diReg[1] = _diReg[1];
			if(__dvd_autofinishing) {
				__dvd_executing->currtxsize = 0;
				DVD_LowRequestAudioStatus(0,__dvd_statebusycb);
			} else {
				__dvd_executing->currtxsize = 1;
				DVD_LowAudioStream(0,__dvd_executing->len,__dvd_executing->offset,__dvd_statebusycb);
			}
			return;
		case 7:
			_diReg[1] = _diReg[1];
			DVD_LowAudioStream(0x00010000,0,0,__dvd_statebusycb);
			return;
		case 8:
			_diReg[1] = _diReg[1];
			__dvd_autofinishing = 1;
			DVD_LowAudioStream(0,0,0,__dvd_statebusycb);
			return;
		case 9:
			_diReg[1] = _diReg[1];
			DVD_LowRequestAudioStatus(0,__dvd_statebusycb);
			return;
		case 10:
			_diReg[1] = _diReg[1];
			DVD_LowRequestAudioStatus(0x00010000,__dvd_statebusycb);
			return;
		case 11:
			_diReg[1] = _diReg[1];
			DVD_LowRequestAudioStatus(0x00020000,__dvd_statebusycb);
			return;
		case 12:
			_diReg[1] = _diReg[1];
			DVD_LowRequestAudioStatus(0x00030000,__dvd_statebusycb);
			return;
		case 13:
			_diReg[1] = _diReg[1];
			DVD_LowAudioBufferConfig(__dvd_executing->offset,__dvd_executing->len,__dvd_statebusycb);
			return;
		case 14:				//Inquiry
			_diReg[1] = _diReg[1];
			block->currtxsize = DVD_DRVINFSIZE;
			DVD_LowInquiry(block->buf,__dvd_statebusycb);
			return;
		case 16:
			_diReg[1] = _diReg[1];
			DVD_LowStopMotor(__dvd_statebusycb);
			return;
		case 17:
			__dvd_lasterror = 0;
			__dvd_extensionsenabled = TRUE;
			DVD_LowSpinUpDrive(__dvd_statebusycb);
			return;
		case 18:
			_diReg[1] = _diReg[1];
			DVD_LowControlMotor(block->offset,__dvd_statebusycb);
			return;
		case 19:
			_diReg[1] = _diReg[1];
			DVD_LowSetGCMOffset(block->offset,__dvd_statebusycb);
			return;
		case 20:
			_diReg[1] = _diReg[1];
			DVD_LowReadImm(block->cmdbuf,__dvd_statebusycb);
			return;
		case 21:
			_diReg[1] = _diReg[1];
			DVD_LowWriteImm(block->cmdbuf,block->immbuf,__dvd_statebusycb);
			return;
		case 22:
			_diReg[1] = _diReg[1];
			DVD_LowReadDma(block->cmdbuf,block->buf,block->currtxsize,__dvd_statebusycb);
			return;
		case 23:
			_diReg[1] = _diReg[1];
			DVD_LowWriteDma(block->cmdbuf,block->buf,block->currtxsize,__dvd_statebusycb);
			return;
		case 24:
			_diReg[1] = _diReg[1];
			block->currtxsize = block->len;
			DVD_GcodeLowRead(block->buf,block->len,block->offset,__dvd_statebusycb);
			return;
		case 25:
			_diReg[1] = _diReg[1];
			len = block->len-block->txdsize;
			if(len<__dvd_gcode_writebufsize) block->currtxsize = len;
			else block->currtxsize = __dvd_gcode_writebufsize;
			DVD_GcodeLowWriteBuffer(block->buf+(DVD_GCODE_BLKSIZE*block->txdsize),block->currtxsize,__dvd_statebusycb);
			return;
		default:
			return;
	}
}

static void __dvd_stateready(void)
{
	dvdcmdblk *block;
#ifdef _DVD_DEBUG
	printf("__dvd_stateready()\n");
#endif
	if(!__dvd_checkwaitingqueue()) {
		__dvd_executing = NULL;
		return;
	}

	if(__dvd_pauseflag) {
		__dvd_pausingflag = 1;
		__dvd_executing = NULL;
		return;
	}

	__dvd_executing = __dvd_popwaitingqueue();

	if(__dvd_fatalerror) {
		__dvd_executing->state = DVD_STATE_FATAL_ERROR;
		block = __dvd_executing;
		__dvd_executing = &__dvd_dummycmdblk;
		if(block->cb) block->cb(DVD_ERROR_FATAL,block);
		__dvd_stateready();
		return;
	}

	__dvd_currcmd = __dvd_executing->cmd;

	if(__dvd_resumefromhere) {
		if(__dvd_resumefromhere<=7) {
			switch(__dvd_resumefromhere) {
				case 1:
					__dvd_executing->state = DVD_STATE_WRONG_DISK;
					__dvd_statemotorstopped();
					break;
				case 2:
					__dvd_executing->state = DVD_STATE_RETRY;
					__dvd_statemotorstopped();
					break;
				case 3:
					__dvd_executing->state = DVD_STATE_NO_DISK;
					__dvd_statemotorstopped();
					break;
				case 4:
					__dvd_executing->state = DVD_STATE_COVER_OPEN;
					__dvd_statemotorstopped();
					break;
				case 5:
					__dvd_executing->state = DVD_STATE_FATAL_ERROR;
					__dvd_stateerror(__dvd_cancellasterror);
					break;
				case 6:
					__dvd_executing->state = DVD_STATE_COVER_CLOSED;
					__dvd_statecoverclosed();
					break;
				case 7:
					__dvd_executing->state = DVD_STATE_MOTOR_STOPPED;
					__dvd_statemotorstopped();
					break;
				default:
					break;

			}
		}
		__dvd_resumefromhere = 0;
		return;
	}
	__dvd_executing->state = DVD_STATE_BUSY;
	__dvd_statebusy(__dvd_executing);
}

static void __dvd_statecoverclosed(void)
{
	dvdcmdblk *blk;
#ifdef _DVD_DEBUG
	printf("__dvd_statecoverclosed(%d)\n",__dvd_currcmd);
#endif
	if(__dvd_currcmd==0x0004 || __dvd_currcmd==0x0005
		|| __dvd_currcmd==0x000d || __dvd_currcmd==0x000f
		|| __dvd_currcmd==0x0011) {
		__dvd_clearwaitingqueue();
		blk = __dvd_executing;
		__dvd_executing = &__dvd_dummycmdblk;
		if(blk->cb) blk->cb(DVD_ERROR_COVER_CLOSED,blk);
		__dvd_stateready();
	} else {
		__dvd_extensionsenabled = TRUE;
		DVD_LowSpinUpDrive(__dvd_statecoverclosed_spinupcb);
	}
}

static void __dvd_statecoverclosed_cmd(dvdcmdblk *block)
{
#ifdef _DVD_DEBUG
	printf("__dvd_statecoverclosed_cmd(%d)\n",__dvd_currcmd);
#endif
	DVD_LowReadDiskID(&__dvd_tmpid0,__dvd_statecoverclosedcb);
}

static void __dvd_statemotorstopped(void)
{
#ifdef _DVD_DEBUG
	printf("__dvd_statemotorstopped(%d)\n",__dvd_executing->state);
#endif
	DVD_LowWaitCoverClose(__dvd_statemotorstoppedcb);
}

static void __dvd_stateerror(s32 result)
{
#ifdef _DVD_DEBUG
	printf("__dvd_stateerror(%08x)\n",result);
#endif
	__dvd_storeerror(result);
	DVD_LowStopMotor(__dvd_stateerrorcb);
}

static void __dvd_stategettingerror(void)
{
#ifdef _DVD_DEBUG
	printf("__dvd_stategettingerror()\n");
#endif
	DVD_LowRequestError(__dvd_stategettingerrorcb);
}

static void __dvd_statecheckid2(dvdcmdblk *block)
{

}

static void __dvd_statecheckid(void)
{
	dvdcmdblk *blk;
#ifdef _DVD_DEBUG
	printf("__dvd_statecheckid(%02x)\n",__dvd_currcmd);
#endif
	if(__dvd_currcmd==0x0003) {
		if(memcmp(__dvd_executing->id,&__dvd_tmpid0,DVD_DISKIDSIZE)) {
			DVD_LowStopMotor(__dvd_statecheckid1cb);
			return;
		}
		memcpy(__dvd_diskID,&__dvd_tmpid0,DVD_DISKIDSIZE);

		__dvd_executing->state = DVD_STATE_BUSY;
		DCInvalidateRange(&__dvd_tmpid0,DVD_DISKIDSIZE);
		__dvd_laststate = __dvd_statecheckid2;
		__dvd_statecheckid2(__dvd_executing);
		return;
	}
	if(__dvd_currcmd==0x0011) {
		blk = __dvd_executing;
		blk->state = DVD_STATE_END;
		__dvd_executing = &__dvd_dummycmdblk;
		if(blk->cb) blk->cb(DVD_ERROR_OK,blk);
		__dvd_stateready();
		return;
	}
	if(__dvd_currcmd!=0x0005 && memcmp(__dvd_diskID,&__dvd_tmpid0,DVD_DISKIDSIZE)) {
		blk = __dvd_executing;
		blk->state = DVD_STATE_FATAL_ERROR;
		__dvd_executing = &__dvd_dummycmdblk;
		if(blk->cb) blk->cb(DVD_ERROR_FATAL,blk);			//terminate current operation
		if(__dvd_mountusrcb) __dvd_mountusrcb(DVD_DISKIDSIZE,blk);		//if we came across here, notify user callback of successful remount
		__dvd_stateready();
		return;
	}
	__dvd_statebusy(__dvd_executing);
}

static void __dvd_statetimeout(void)
{
	__dvd_storeerror(0x01234568);
	DVD_Reset(DVD_RESETSOFT);
	__dvd_stateerrorcb(0);
}

static void __dvd_stategotoretry(void)
{
	DVD_LowStopMotor(__dvd_stategotoretrycb);
}

static s32 __issuecommand(s32 prio,dvdcmdblk *block)
{
	s32 ret;
	u32 level;
#ifdef _DVD_DEBUG
	printf("__issuecommand(%d,%p,%p)\n",prio,block,block->cb);
	printf("__issuecommand(%p)\n",__dvd_waitingqueue[prio].first);
#endif
	if(__dvd_autoinvalidation) {
		if(block->cmd==0x0001 || block->cmd==0x0004
		|| block->cmd==0x0005 || block->cmd==0x000e
		|| block->cmd==0x0018) DCInvalidateRange(block->buf,block->len);
		else if(block->cmd==0x0019) DCStoreRange(block->buf,block->len*DVD_GCODE_BLKSIZE);
		else if(block->cmd==0x0016) DCInvalidateRange(block->buf,block->currtxsize);
		else if(block->cmd==0x0017) DCStoreRange(block->buf,block->currtxsize);
	}

	_CPU_ISR_Disable(level);
	block->state = DVD_STATE_WAITING;
	ret = __dvd_pushwaitingqueue(prio,block);
	if(!__dvd_executing && !__dvd_pauseflag) __dvd_stateready();
	_CPU_ISR_Restore(level);
	return ret;
}

s32 DVD_LowUnlockDrive(dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_LowUnlockDrive()\n");
#endif
	u32 i;

	__dvd_callback = __dvd_unlockdrivecb;
	__dvd_finalunlockcb = cb;
	__dvd_stopnextint = 0;

	for(i=0;i<3;i++) _diReg[2+i] = ((const u32*)__dvd_unlockcmd$221)[i];
	_diReg[7] = DVD_DI_START;

	return 1;
}

s32 DVD_LowPatchDriveCode(dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_LowPatchDriveCode(%08x)\n",__dvd_driveinfo.rel_date);
#endif
	__dvd_finalpatchcb = cb;
	__dvd_stopnextint = 0;

	if(__dvd_driveinfo.rel_date==DVD_MODEL04) {
		__dvdpatchcode = __dvd_patchcode04;
		__dvdpatchcode_size = __dvd_patchcode04_size;
	} else if(__dvd_driveinfo.rel_date==DVD_MODEL06) {
		__dvdpatchcode = __dvd_patchcode06;
		__dvdpatchcode_size = __dvd_patchcode06_size;
	} else if(__dvd_driveinfo.rel_date==DVD_MODEL08) {
		__dvdpatchcode = __dvd_patchcode08;
		__dvdpatchcode_size = __dvd_patchcode08_size;
	} else if(__dvd_driveinfo.rel_date==DVD_MODEL08Q) {
		__dvdpatchcode = __dvd_patchcodeQ08;
		__dvdpatchcode_size = __dvd_patchcodeQ08_size;
	} else {
		__dvdpatchcode = NULL;
		__dvdpatchcode_size = 0;
	}

	__dvd_patchdrivecb(0);
	return 1;
}

s32 DVD_LowSetOffset(s64 offset,dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_LowSetOffset(%08x)\n",offset);
#endif
	__dvd_stopnextint = 0;
	__dvd_callback = cb;

	_diReg[2] = DVD_FWSETOFFSET;
	_diReg[3] = (u32)(offset>>2);
	_diReg[4] = 0;
	_diReg[7] = DVD_DI_START;

	return 1;
}

s32 DVD_LowSetGCMOffset(s64 offset,dvdcallbacklow cb)
{
	static s64 loc_offset = 0;
#ifdef _DVD_DEBUG
	printf("DVD_LowSetGCMOffset(%08x)\n",offset);
#endif
	loc_offset = offset;

	__dvd_finaloffsetcb = cb;
	__dvd_stopnextint = 0;
	__dvd_usrdata = &loc_offset;

	DVD_LowUnlockDrive(__dvd_setgcmoffsetcb);
	return 1;
}

s32 DVD_LowReadmem(u32 address,void *buffer,dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_LowReadmem(0x%08x,%p)\n",address,buffer);
#endif
	__dvd_finalreadmemcb = cb;
	__dvd_usrdata = buffer;
	__dvd_callback = __dvd_readmemcb;
	__dvd_stopnextint = 0;

	_diReg[2] = DVD_FWREADMEM;
	_diReg[3] = address;
	_diReg[4] = 0x00010000;
	_diReg[7] = DVD_DI_START;

	return 1;
}

s32 DVD_LowFuncCall(u32 address,dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_LowFuncCall(0x%08x)\n",address);
#endif
	__dvd_callback = cb;
	__dvd_stopnextint = 0;

	_diReg[2] = DVD_FWFUNCCALL;
	_diReg[3] = address;
	_diReg[4] = 0x66756E63;
	_diReg[7] = DVD_DI_START;

	return 1;
}

s32 DVD_LowSpinMotor(u32 mode,dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_LowSpinMotor(%08x)\n",mode);
#endif
	__dvd_callback = cb;
	__dvd_stopnextint = 0;

	_diReg[2] = DVD_FWCTRLMOTOR|(mode&0x0000ff00);
	_diReg[3] = 0;
	_diReg[4] = 0;
	_diReg[7] = DVD_DI_START;

	return 1;
}

s32 DVD_LowEnableExtensions(u8 enable,dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_LowEnableExtensions(%02x)\n",enable);
#endif
	__dvd_callback = cb;
	__dvd_stopnextint = 0;

	_diReg[2] = (DVD_FWENABLEEXT|_SHIFTL(enable,16,8));
	_diReg[3] = 0;
	_diReg[4] = 0;
	_diReg[7] = DVD_DI_START;

	return 1;
}

s32 DVD_LowSetStatus(u32 status,dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_LowSetStatus(%08x)\n",status);
#endif
	__dvd_callback = cb;
	__dvd_stopnextint = 0;

	_diReg[2] = (DVD_FWSETSTATUS|(status&0x00ffffff));
	_diReg[3] = 0;
	_diReg[4] = 0;
	_diReg[7] = DVD_DI_START;

	return 1;
}

s32 DVD_LowGetStatus(u32 *status,dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_LowGetStatus(%p)\n",status);
#endif
	__dvd_finalstatuscb = cb;
	__dvd_usrdata = status;

	DVD_LowRequestError(__dvd_getstatuscb);
	return 1;
}

s32 DVD_LowControlMotor(u32 mode,dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_LowControlMotor(%08x)\n",mode);
#endif
	__dvd_stopnextint = 0;
	__dvd_motorcntrl = mode;
	__dvd_finalsudcb = cb;
	DVD_LowUnlockDrive(__dvd_cntrldrivecb);

	return 1;

}

s32 DVD_LowSpinUpDrive(dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_LowSpinUpDrive()\n");
#endif
	__dvd_finalsudcb = cb;
	__dvd_spinupdrivecb(1);

	return 1;
}

s32 DVD_LowReadImm(dvdcmdbuf cmdbuf,dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_LowReadImm(%p)\n",cmdbuf);
#endif
	struct timespec tb;

	__dvd_callback = cb;
	__dvd_stopnextint = 0;

	_diReg[2] = cmdbuf[0];
	_diReg[3] = cmdbuf[1];
	_diReg[4] = cmdbuf[2];
	_diReg[7] = DVD_DI_START;

	tb.tv_sec = 10;
	tb.tv_nsec = 0;
	__SetupTimeoutAlarm(&tb);

	return 1;
}

s32 DVD_LowWriteImm(dvdcmdbuf cmdbuf,u32 immbuf,dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_LowWriteImm(%p,%08x)\n",cmdbuf,immbuf);
#endif
	struct timespec tb;

	__dvd_callback = cb;
	__dvd_stopnextint = 0;

	_diReg[2] = cmdbuf[0];
	_diReg[3] = cmdbuf[1];
	_diReg[4] = cmdbuf[2];
	_diReg[8] = immbuf;
	_diReg[7] = (DVD_DI_MODE|DVD_DI_START);

	tb.tv_sec = 10;
	tb.tv_nsec = 0;
	__SetupTimeoutAlarm(&tb);

	return 1;
}

s32 DVD_LowReadDma(dvdcmdbuf cmdbuf,void *buf,u32 len,dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_LowReadDma(%p,%p,%d)\n",cmdbuf,buf,len);
#endif
	struct timespec tb;

	__dvd_callback = cb;
	__dvd_stopnextint = 0;

	_diReg[2] = cmdbuf[0];
	_diReg[3] = cmdbuf[1];
	_diReg[4] = cmdbuf[2];
	_diReg[5] = (u32)buf;
	_diReg[6] = len;
	_diReg[7] = (DVD_DI_DMA|DVD_DI_START);

	tb.tv_sec = 10;
	tb.tv_nsec = 0;
	__SetupTimeoutAlarm(&tb);

	return 1;
}

s32 DVD_LowWriteDma(dvdcmdbuf cmdbuf,void *buf,u32 len,dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_LowWriteDma(%p,%p,%d)\n",cmdbuf,buf,len);
#endif
	struct timespec tb;

	__dvd_callback = cb;
	__dvd_stopnextint = 0;

	_diReg[2] = cmdbuf[0];
	_diReg[3] = cmdbuf[1];
	_diReg[4] = cmdbuf[2];
	_diReg[5] = (u32)buf;
	_diReg[6] = len;
	_diReg[7] = (DVD_DI_MODE|DVD_DI_DMA|DVD_DI_START);

	tb.tv_sec = 10;
	tb.tv_nsec = 0;
	__SetupTimeoutAlarm(&tb);

	return 1;
}

s32 DVD_GcodeLowRead(void *buf,u32 len,u32 offset,dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_GcodeLowRead(%p,%d,%d)\n",buf,len,offset);
#endif
	struct timespec tb;

	__dvd_callback = cb;
	__dvd_stopnextint = 0;

	_diReg[2] = DVD_GCODE_READ;
	_diReg[3] = offset;
	_diReg[4] = len;
	_diReg[5] = (u32)buf;
	_diReg[6] = len;
	_diReg[7] = (DVD_DI_DMA|DVD_DI_START);

	tb.tv_sec = 10;
	tb.tv_nsec = 0;
	__SetupTimeoutAlarm(&tb);

	return 1;
}

s32 DVD_GcodeLowWriteBuffer(void *buf,u32 len,dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_GcodeLowWriteBuffer(%p,%d)\n",buf,len);
#endif
	struct timespec tb;

	__dvd_callback = cb;
	__dvd_stopnextint = 0;

	if(len==1) {
		_diReg[2] = DVD_GCODE_WRITEBUFFER;
		_diReg[3] = 0;
		_diReg[4] = 0;
		_diReg[5] = (u32)buf;
		_diReg[6] = DVD_GCODE_BLKSIZE;
		_diReg[7] = (DVD_DI_MODE|DVD_DI_DMA|DVD_DI_START);
	} else {
		_diReg[2] = DVD_GCODE_WRITEBUFFEREX;
		_diReg[3] = 0;
		_diReg[4] = (DVD_GCODE_BLKSIZE*len);
		_diReg[5] = (u32)buf;
		_diReg[6] = (DVD_GCODE_BLKSIZE*len);
		_diReg[7] = (DVD_DI_MODE|DVD_DI_DMA|DVD_DI_START);
	}

	tb.tv_sec = 10;
	tb.tv_nsec = 0;
	__SetupTimeoutAlarm(&tb);

	return 1;
}

s32 DVD_GcodeLowWrite(u32 len,u32 offset,dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_GcodeLowWrite(%d,%d)\n",len,offset);
#endif
	struct timespec tb;

	__dvd_callback = cb;
	__dvd_stopnextint = 0;

	if(len==1) {
		_diReg[2] = DVD_GCODE_WRITE;
		_diReg[3] = offset;
		_diReg[7] = DVD_DI_START;
	} else {
		_diReg[2] = DVD_GCODE_WRITEEX;
		_diReg[3] = offset;
		_diReg[4] = len;
		_diReg[7] = DVD_DI_START;
	}

	tb.tv_sec = 10;
	tb.tv_nsec = 0;
	__SetupTimeoutAlarm(&tb);

	return 1;
}

s32 DVD_LowRead(void *buf,u32 len,s64 offset,dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_LowRead(%p,%d,%d)\n",buf,len,offset);
#endif
	_diReg[6] = len;

	__dvd_cmd_curr.buf = buf;
	__dvd_cmd_curr.len = len;
	__dvd_cmd_curr.offset = offset;
	__DoRead(buf,len,offset,cb);
	return 1;
}

s32 DVD_LowSeek(s64 offset,dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_LowSeek(%d)\n",offset);
#endif
	struct timespec tb;

	__dvd_callback = cb;
	__dvd_stopnextint = 0;

	_diReg[2] = DVD_SEEK;
	_diReg[3] = (u32)(offset>>2);
	_diReg[7] = DVD_DI_START;

	tb.tv_sec = 10;
	tb.tv_nsec = 0;
	__SetupTimeoutAlarm(&tb);

	return 1;
}

s32 DVD_LowReadDiskID(dvddiskid *diskID,dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_LowReadDiskID(%p)\n",diskID);
#endif
	struct timespec tb;

	__dvd_callback = cb;
	__dvd_stopnextint = 0;

	_diReg[2] = DVD_READDISKID;
	_diReg[3] = 0;
	_diReg[4] = DVD_DISKIDSIZE;
	_diReg[5] = (u32)diskID;
	_diReg[6] = DVD_DISKIDSIZE;
	_diReg[7] = (DVD_DI_DMA|DVD_DI_START);

	tb.tv_sec = 10;
	tb.tv_nsec = 0;
	__SetupTimeoutAlarm(&tb);

	return 1;
}

s32 DVD_LowRequestError(dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_LowRequestError()\n");
#endif
	struct timespec tb;

	__dvd_callback = cb;
	__dvd_stopnextint = 0;

	_diReg[2] = DVD_REQUESTERROR;
	_diReg[7] = DVD_DI_START;

	tb.tv_sec = 10;
	tb.tv_nsec = 0;
	__SetupTimeoutAlarm(&tb);

	return 1;
}

s32 DVD_LowStopMotor(dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_LowStopMotor()\n");
#endif
	struct timespec tb;

	__dvd_callback = cb;
	__dvd_stopnextint = 0;

	_diReg[2] = DVD_STOPMOTOR;
	_diReg[7] = DVD_DI_START;

	tb.tv_sec = 10;
	tb.tv_nsec = 0;
	__SetupTimeoutAlarm(&tb);

	return 1;
}

s32 DVD_LowInquiry(dvddrvinfo *info,dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_LowInquiry(%p)\n",info);
#endif
	struct timespec tb;

	__dvd_callback = cb;
	__dvd_stopnextint = 0;

	_diReg[2] = DVD_INQUIRY;
	_diReg[4] = DVD_DRVINFSIZE;
	_diReg[5] = (u32)info;
	_diReg[6] = DVD_DRVINFSIZE;
	_diReg[7] = (DVD_DI_DMA|DVD_DI_START);

	tb.tv_sec = 10;
	tb.tv_nsec = 0;
	__SetupTimeoutAlarm(&tb);

	return 1;
}

s32 DVD_LowWaitCoverClose(dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_LowWaitCoverClose()\n");
#endif
	__dvd_callback = cb;
	__dvd_waitcoverclose = 1;
	__dvd_stopnextint = 0;
	_diReg[1] = DVD_CVR_MSK;
	return 1;
}

s32 DVD_LowGetCoverStatus(void)
{
	s64 now;
	u32 diff;

	now = __SYS_GetSystemTime();
	diff = diff_msec(__dvd_lastresetend,now);
	if(diff<100) return 0;
	else if(_diReg[1]&DVD_CVR_STATE) return 1;
	else return 2;
}

void DVD_LowReset(u32 reset_mode)
{
	u32 val;

	_diReg[1] = DVD_CVR_MSK;
	val = _piReg[9];
	_piReg[9] = ((val&~0x0004)|0x0001);

	udelay(12);

	if(reset_mode==DVD_RESETHARD) val |= 0x0004;
	val |= 0x0001;
	_piReg[9] = val;

	__dvd_resetoccured = 1;
	__dvd_lastresetend = __SYS_GetSystemTime();
	__dvd_drivestate |= DVD_DRIVERESET;
}

dvdcallbacklow DVD_LowSetResetCoverCallback(dvdcallbacklow cb)
{
	u32 level;
	dvdcallbacklow old;

	_CPU_ISR_Disable(level);
	old = __dvd_resetcovercb;
	__dvd_resetcovercb = cb;
	_CPU_ISR_Restore(level);
	return old;
}

s32 DVD_LowBreak(void)
{
	__dvd_stopnextint = 1;
	__dvd_breaking = 1;
	return 1;
}

dvdcallbacklow DVD_LowClearCallback(void)
{
	dvdcallbacklow old;

	_diReg[1] = 0;
	__dvd_waitcoverclose = 0;

	old = __dvd_callback;
	__dvd_callback = NULL;
	return old;
}

s32 DVD_LowAudioStream(u32 subcmd,u32 len,s64 offset,dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_LowAudioStream(%08x,%d,%08x,%p)\n",subcmd,len,offset,cb);
#endif
	struct timespec tb;

	__dvd_callback = cb;
	__dvd_stopnextint = 0;

	_diReg[2] = DVD_AUDIOSTREAM|subcmd;
	_diReg[3] = (u32)(offset>>2);
	_diReg[4] = len;
	_diReg[7] = DVD_DI_START;

	tb.tv_sec = 10;
	tb.tv_nsec = 0;
	__SetupTimeoutAlarm(&tb);

	return 1;
}

s32 DVD_LowAudioBufferConfig(s32 enable,u32 size,dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_LowAudioBufferConfig(%02x,%d,%p)\n",enable,size,cb);
#endif
	u32 val;
	struct timespec tb;

	__dvd_callback = cb;
	__dvd_stopnextint = 0;

	val = 0;
	if(enable) {
		val |= 0x00010000;
		if(!size) val |= 0x0000000a;
	}

	_diReg[2] = DVD_AUDIOCONFIG|val;
	_diReg[7] = DVD_DI_START;

	tb.tv_sec = 10;
	tb.tv_nsec = 0;
	__SetupTimeoutAlarm(&tb);

	return 1;
}

s32 DVD_LowRequestAudioStatus(u32 subcmd,dvdcallbacklow cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_LowRequestAudioStatus(%08x,%p)\n",subcmd,cb);
#endif
	struct timespec tb;

	__dvd_callback = cb;
	__dvd_stopnextint = 0;

	_diReg[2] = DVD_AUDIOSTATUS|subcmd;
	_diReg[7] = DVD_DI_START;

	tb.tv_sec = 10;
	tb.tv_nsec = 0;
	__SetupTimeoutAlarm(&tb);

	return 1;
}

//special, only used in bios replacement. therefor only there extern'd
s32 __DVDAudioBufferConfig(dvdcmdblk *block,u32 enable,u32 size,dvdcbcallback cb)
{
	if(!block) return 0;

	block->cmd = 0x000d;
	block->offset = enable;
	block->len = size;
	block->cb = cb;

	return __issuecommand(2,block);
}

s32 DVD_ReadDiskID(dvdcmdblk *block,dvddiskid *id,dvdcbcallback cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_ReadDiskID(%p,%p)\n",block,id);
#endif
	if(!block || !id) return 0;

	block->cmd = 0x0005;
	block->buf = (void*)id;
	block->len = DVD_DISKIDSIZE;
	block->offset = 0;
	block->txdsize = 0;
	block->cb = cb;

	return __issuecommand(2,block);
}

s32 DVD_ReadAbsAsyncPrio(dvdcmdblk *block,void *buf,u32 len,s64 offset,dvdcbcallback cb,s32 prio)
{
#ifdef _DVD_DEBUG
	printf("DVD_ReadAbsAsyncPrio(%p,%p,%d,%d,%p,%d)\n",block,buf,len,offset,cb,prio);
#endif
	block->cmd = 0x0001;
	block->buf = (void*)buf;
	block->len = len;
	block->offset = offset;
	block->txdsize = 0;
	block->cb = cb;

	return __issuecommand(prio,block);
}

s32 DVD_ReadAbsAsyncForBS(dvdcmdblk *block,void *buf,u32 len,s64 offset,dvdcbcallback cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_ReadAbsAsyncForBS(%p,%p,%d,%d,%p)\n",block,buf,len,offset,cb);
#endif
	block->cmd = 0x0004;
	block->buf = (void*)buf;
	block->len = len;
	block->offset = offset;
	block->txdsize = 0;
	block->cb = cb;

	return __issuecommand(2,block);
}

s32 DVD_SeekAbsAsyncPrio(dvdcmdblk *block,s64 offset,dvdcbcallback cb,s32 prio)
{
#ifdef _DVD_DEBUG
	printf("DVD_SeekAbsAsyncPrio(%p,%d,%p,%d)\n",block,offset,cb,prio);
#endif
	block->cmd = 0x0002;
	block->offset = offset;
	block->cb = cb;

	return __issuecommand(prio,block);
}

s32 DVD_InquiryAsync(dvdcmdblk *block,dvddrvinfo *info,dvdcbcallback cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_InquiryAsync(%p,%p,%p)\n",block,info,cb);
#endif
	block->cmd = 0x000e;
	block->buf = (void*)info;
	block->len = DVD_DRVINFSIZE;
	block->txdsize = 0;
	block->cb = cb;

	return __issuecommand(2,block);
}

s32 DVD_Inquiry(dvdcmdblk *block,dvddrvinfo *info)
{
	s32 ret,state;
	u32 level;
#ifdef _DVD_DEBUG
	printf("DVD_Inquiry(%p,%p)\n",block,info);
#endif
	ret = DVD_InquiryAsync(block,info,__dvd_synccallback);
	if(!ret) return DVD_ERROR_FATAL;

	_CPU_ISR_Disable(level);
	do {
		state = block->state;
		if(state==DVD_STATE_END) ret = block->txdsize;
		else if(state==DVD_STATE_FATAL_ERROR) ret = DVD_ERROR_FATAL;
		else if(state==DVD_STATE_CANCELED) ret = DVD_ERROR_CANCELED;
		else LWP_ThreadSleep(__dvd_wait_queue);
	} while(state!=DVD_STATE_END && state!=DVD_STATE_FATAL_ERROR && state!=DVD_STATE_CANCELED);
	_CPU_ISR_Restore(level);

	return ret;
}

s32 DVD_ReadAbsPrio(dvdcmdblk *block,void *buf,u32 len,s64 offset,s32 prio)
{
	s32 ret,state;
	u32 level;
#ifdef _DVD_DEBUG
	printf("DVD_ReadAbsPrio(%p,%p,%d,%d,%d)\n",block,buf,len,offset,prio);
#endif
	if(offset>=0 && offset<8511160320LL) {
		ret = DVD_ReadAbsAsyncPrio(block,buf,len,offset,__dvd_synccallback,prio);
		if(!ret) return DVD_ERROR_FATAL;

		_CPU_ISR_Disable(level);
		do {
			state = block->state;
			if(state==DVD_STATE_END) ret = block->txdsize;
			else if(state==DVD_STATE_FATAL_ERROR) ret = DVD_ERROR_FATAL;
			else if(state==DVD_STATE_CANCELED) ret = DVD_ERROR_CANCELED;
			else LWP_ThreadSleep(__dvd_wait_queue);
		} while(state!=DVD_STATE_END && state!=DVD_STATE_FATAL_ERROR && state!=DVD_STATE_CANCELED);
		_CPU_ISR_Restore(level);

		return ret;
	}
	return DVD_ERROR_FATAL;
}

s32 DVD_SeekAbsPrio(dvdcmdblk *block,s64 offset,s32 prio)
{
	s32 ret,state;
	u32 level;
#ifdef _DVD_DEBUG
	printf("DVD_SeekAbsPrio(%p,%d,%d)\n",block,offset,prio);
#endif
	if(offset>=0 && offset<8511160320LL) {
		ret = DVD_SeekAbsAsyncPrio(block,offset,__dvd_synccallback,prio);
		if(!ret) return DVD_ERROR_FATAL;

		_CPU_ISR_Disable(level);
		do {
			state = block->state;
			if(state==DVD_STATE_END) ret = DVD_ERROR_OK;
			else if(state==DVD_STATE_FATAL_ERROR) ret = DVD_ERROR_FATAL;
			else if(state==DVD_STATE_CANCELED) ret = DVD_ERROR_CANCELED;
			else LWP_ThreadSleep(__dvd_wait_queue);
		} while(state!=DVD_STATE_END && state!=DVD_STATE_FATAL_ERROR && state!=DVD_STATE_CANCELED);
		_CPU_ISR_Restore(level);

		return ret;
	}
	return DVD_ERROR_FATAL;
}

s32 DVD_CancelAsync(dvdcmdblk *block,dvdcbcallback cb)
{
	u32 level;
	dvdcallbacklow old;
#ifdef _DVD_DEBUG
	printf("DVD_CancelAsync(%p,%p)\n",block,cb);
#endif
	_CPU_ISR_Disable(level);

	switch(block->state) {
		case DVD_STATE_FATAL_ERROR:
		case DVD_STATE_END:
		case DVD_STATE_CANCELED:
			if(cb) cb(DVD_ERROR_OK,block);
			break;
		case DVD_STATE_BUSY:
			if(__dvd_canceling) {
				_CPU_ISR_Restore(level);
				return 0;
			}
			__dvd_canceling = 1;
			__dvd_cancelcallback = cb;
			if(__dvd_currcmd==0x0001 || __dvd_currcmd==0x0004) DVD_LowBreak();
			break;
		case DVD_STATE_WAITING:
			__dvd_dequeuewaitingqueue(block);
			block->state = DVD_STATE_CANCELED;
			if(block->cb) block->cb(DVD_ERROR_CANCELED,block);
			if(cb) cb(DVD_ERROR_OK,block);
			break;
		case DVD_STATE_COVER_CLOSED:
			if(__dvd_currcmd==0x0004 || __dvd_currcmd==0x0005
				|| __dvd_currcmd==0x000d || __dvd_currcmd==0x000f) {
				if(cb) cb(DVD_ERROR_OK,block);
				break;
			}
			if(__dvd_canceling) {
				_CPU_ISR_Restore(level);
				return 0;
			}
			__dvd_canceling = 1;
			__dvd_cancelcallback = cb;
			break;
		case DVD_STATE_NO_DISK:
		case DVD_STATE_COVER_OPEN:
		case DVD_STATE_WRONG_DISK:
		case DVD_STATE_MOTOR_STOPPED:
		case DVD_STATE_RETRY:
			old = DVD_LowClearCallback();
			if(old!=__dvd_statemotorstoppedcb) {
				_CPU_ISR_Restore(level);
				return 0;
			}
			switch(block->state) {
				case DVD_STATE_WRONG_DISK:
					__dvd_resumefromhere = 1;
					break;
				case DVD_STATE_RETRY:
					__dvd_resumefromhere = 2;
					break;
				case DVD_STATE_NO_DISK:
					__dvd_resumefromhere = 3;
					break;
				case DVD_STATE_COVER_OPEN:
					__dvd_resumefromhere = 4;
					break;
				case DVD_STATE_MOTOR_STOPPED:
					__dvd_resumefromhere = 7;
					break;
				default:
					break;
			}
			__dvd_executing = &__dvd_dummycmdblk;
			block->state = DVD_STATE_CANCELED;
			if(block->cb) block->cb(DVD_ERROR_CANCELED,block);
			if(cb) cb(DVD_ERROR_OK,block);
			__dvd_stateready();
			break;
		default:
			break;
	}
	_CPU_ISR_Restore(level);
	return 1;
}

s32 DVD_CancelAllAsync(dvdcbcallback cb)
{
	s32 ret;
	u32 level;
	dvdcmdblk *block;
#ifdef _DVD_DEBUG
	printf("DVD_CancelAllAsync(%p)\n",cb);
#endif
	_CPU_ISR_Disable(level);
	DVD_Pause();
	while((block=__dvd_popwaitingqueue())) DVD_CancelAsync(block,NULL);
	if(__dvd_executing) ret = DVD_CancelAsync(__dvd_executing,cb);
	else {
		ret = 1;
		if(cb) cb(DVD_ERROR_OK,NULL);
	}
	DVD_Resume();
	_CPU_ISR_Restore(level);
	return ret;
}

s32 DVD_PrepareStreamAbsAsync(dvdcmdblk *block,u32 len,s64 offset,dvdcbcallback cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_PrepareStreamAbsAsync(%p,%d,%d,%p)\n",block,len,offset,cb);
#endif
	if((len&0x7fff) || (offset&0x7fff)) return 0;

	block->cmd = 0x0006;
	block->len = len;
	block->offset = offset;
	block->cb = cb;

	return __issuecommand(1,block);
}

s32 DVD_PrepareStreamAbs(dvdcmdblk *block,u32 len,s64 offset)
{
	s32 ret,state;
	u32 level;
#ifdef _DVD_DEBUG
	printf("DVD_PrepareStreamAbs(%p,%d,%d)\n",block,len,offset);
#endif
	ret = DVD_PrepareStreamAbsAsync(block,len,offset,__dvd_synccallback);
	if(!ret) return DVD_ERROR_FATAL;

	_CPU_ISR_Disable(level);
	do {
		state = block->state;
		if(state==DVD_STATE_END) ret = DVD_ERROR_OK;
		else if(state==DVD_STATE_FATAL_ERROR) ret = DVD_ERROR_FATAL;
		else if(state==DVD_STATE_CANCELED) ret = DVD_ERROR_CANCELED;
		else LWP_ThreadSleep(__dvd_wait_queue);
	} while(state!=DVD_STATE_END && state!=DVD_STATE_FATAL_ERROR && state!=DVD_STATE_CANCELED);
	_CPU_ISR_Restore(level);

	return ret;
}

s32 DVD_CancelStreamAsync(dvdcmdblk *block,dvdcbcallback cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_CancelStreamAsync(%p,%p)\n",block,cb);
#endif
	block->cmd = 0x0007;
	block->cb = cb;
	return __issuecommand(1,block);
}

s32 DVD_CancelStream(dvdcmdblk *block)
{
	s32 ret,state;
	u32 level;
#ifdef _DVD_DEBUG
	printf("DVD_CancelStream(%p)\n",block);
#endif
	ret = DVD_CancelStreamAsync(block,__dvd_synccallback);
	if(!ret) return DVD_ERROR_FATAL;

	_CPU_ISR_Disable(level);
	do {
		state = block->state;
		if(state==DVD_STATE_END) ret = DVD_ERROR_OK;
		else if(state==DVD_STATE_FATAL_ERROR) ret = DVD_ERROR_FATAL;
		else if(state==DVD_STATE_CANCELED) ret = DVD_ERROR_CANCELED;
		else LWP_ThreadSleep(__dvd_wait_queue);
	} while(state!=DVD_STATE_END && state!=DVD_STATE_FATAL_ERROR && state!=DVD_STATE_CANCELED);
	_CPU_ISR_Restore(level);

	return ret;
}

s32 DVD_StopStreamAtEndAsync(dvdcmdblk *block,dvdcbcallback cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_StopStreamAtEndAsync(%p,%p)\n",block,cb);
#endif
	block->cmd = 0x0008;
	block->cb = cb;
	return __issuecommand(1,block);
}

s32 DVD_StopStreamAtEnd(dvdcmdblk *block)
{
	s32 ret,state;
	u32 level;
#ifdef _DVD_DEBUG
	printf("DVD_StopStreamAtEnd(%p)\n",block);
#endif
	ret = DVD_StopStreamAtEndAsync(block,__dvd_synccallback);
	if(!ret) return DVD_ERROR_FATAL;

	_CPU_ISR_Disable(level);
	do {
		state = block->state;
		if(state==DVD_STATE_END) ret = DVD_ERROR_OK;
		else if(state==DVD_STATE_FATAL_ERROR) ret = DVD_ERROR_FATAL;
		else if(state==DVD_STATE_CANCELED) ret = DVD_ERROR_CANCELED;
		else LWP_ThreadSleep(__dvd_wait_queue);
	} while(state!=DVD_STATE_END && state!=DVD_STATE_FATAL_ERROR && state!=DVD_STATE_CANCELED);
	_CPU_ISR_Restore(level);

	return ret;
}

s32 DVD_GetStreamErrorStatusAsync(dvdcmdblk *block,dvdcbcallback cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_GetStreamErrorStatusAsync(%p,%p)\n",block,cb);
#endif
	block->cmd = 0x0009;
	block->cb = cb;
	return __issuecommand(1,block);
}

s32 DVD_GetStreamErrorStatus(dvdcmdblk *block)
{
	s32 ret,state;
	u32 level;
#ifdef _DVD_DEBUG
	printf("DVD_GetStreamErrorStatus(%p)\n",block);
#endif
	ret = DVD_GetStreamErrorStatusAsync(block,__dvd_synccallback);
	if(!ret) return DVD_ERROR_FATAL;

	_CPU_ISR_Disable(level);
	do {
		state = block->state;
		if(state==DVD_STATE_END) ret = block->txdsize;
		else if(state==DVD_STATE_FATAL_ERROR) ret = DVD_ERROR_FATAL;
		else if(state==DVD_STATE_CANCELED) ret = DVD_ERROR_CANCELED;
		else LWP_ThreadSleep(__dvd_wait_queue);
	} while(state!=DVD_STATE_END && state!=DVD_STATE_FATAL_ERROR && state!=DVD_STATE_CANCELED);
	_CPU_ISR_Restore(level);

	return ret;
}

s32 DVD_GetStreamPlayAddrAsync(dvdcmdblk *block,dvdcbcallback cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_GetStreamPlayAddrAsync(%p,%p)\n",block,cb);
#endif
	block->cmd = 0x000a;
	block->cb = cb;
	return __issuecommand(1,block);
}

s32 DVD_GetStreamPlayAddr(dvdcmdblk *block)
{
	s32 ret,state;
	u32 level;
#ifdef _DVD_DEBUG
	printf("DVD_GetStreamPlayAddr(%p)\n",block);
#endif
	ret = DVD_GetStreamPlayAddrAsync(block,__dvd_synccallback);
	if(!ret) return DVD_ERROR_FATAL;

	_CPU_ISR_Disable(level);
	do {
		state = block->state;
		if(state==DVD_STATE_END) ret = block->txdsize;
		else if(state==DVD_STATE_FATAL_ERROR) ret = DVD_ERROR_FATAL;
		else if(state==DVD_STATE_CANCELED) ret = DVD_ERROR_CANCELED;
		else LWP_ThreadSleep(__dvd_wait_queue);
	} while(state!=DVD_STATE_END && state!=DVD_STATE_FATAL_ERROR && state!=DVD_STATE_CANCELED);
	_CPU_ISR_Restore(level);

	return ret;
}

s32 DVD_GetStreamStartAddrAsync(dvdcmdblk *block,dvdcbcallback cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_GetStreamStartAddrAsync(%p,%p)\n",block,cb);
#endif
	block->cmd = 0x000b;
	block->cb = cb;
	return __issuecommand(1,block);
}

s32 DVD_GetStreamStartAddr(dvdcmdblk *block)
{
	s32 ret,state;
	u32 level;
#ifdef _DVD_DEBUG
	printf("DVD_GetStreamStartAddr(%p)\n",block);
#endif
	ret = DVD_GetStreamStartAddrAsync(block,__dvd_synccallback);
	if(!ret) return DVD_ERROR_FATAL;

	_CPU_ISR_Disable(level);
	do {
		state = block->state;
		if(state==DVD_STATE_END) ret = block->txdsize;
		else if(state==DVD_STATE_FATAL_ERROR) ret = DVD_ERROR_FATAL;
		else if(state==DVD_STATE_CANCELED) ret = DVD_ERROR_CANCELED;
		else LWP_ThreadSleep(__dvd_wait_queue);
	} while(state!=DVD_STATE_END && state!=DVD_STATE_FATAL_ERROR && state!=DVD_STATE_CANCELED);
	_CPU_ISR_Restore(level);

	return ret;
}

s32 DVD_GetStreamLengthAsync(dvdcmdblk *block,dvdcbcallback cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_GetStreamLengthAsync(%p,%p)\n",block,cb);
#endif
	block->cmd = 0x000c;
	block->cb = cb;
	return __issuecommand(1,block);
}

s32 DVD_GetStreamLength(dvdcmdblk *block)
{
	s32 ret,state;
	u32 level;
#ifdef _DVD_DEBUG
	printf("DVD_GetStreamLength(%p)\n",block);
#endif
	ret = DVD_GetStreamLengthAsync(block,__dvd_synccallback);
	if(!ret) return DVD_ERROR_FATAL;

	_CPU_ISR_Disable(level);
	do {
		state = block->state;
		if(state==DVD_STATE_END) ret = block->txdsize;
		else if(state==DVD_STATE_FATAL_ERROR) ret = DVD_ERROR_FATAL;
		else if(state==DVD_STATE_CANCELED) ret = DVD_ERROR_CANCELED;
		else LWP_ThreadSleep(__dvd_wait_queue);
	} while(state!=DVD_STATE_END && state!=DVD_STATE_FATAL_ERROR && state!=DVD_STATE_CANCELED);
	_CPU_ISR_Restore(level);

	return ret;
}

s32 DVD_StopMotorAsync(dvdcmdblk *block,dvdcbcallback cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_StopMotorAsync(%p,%p)\n",block,cb);
#endif
	block->cmd = 0x0010;
	block->cb = cb;
	return __issuecommand(2,block);
}

s32 DVD_StopMotor(dvdcmdblk *block)
{
	s32 ret,state;
	u32 level;
#ifdef _DVD_DEBUG
	printf("DVD_StopMotor(%p)\n",block);
#endif
	ret = DVD_StopMotorAsync(block,__dvd_synccallback);
	if(!ret) return DVD_ERROR_FATAL;

	_CPU_ISR_Disable(level);
	do {
		state = block->state;
		if(state==DVD_STATE_END) ret = DVD_ERROR_OK;
		else if(state==DVD_STATE_FATAL_ERROR) ret = DVD_ERROR_FATAL;
		else if(state==DVD_STATE_CANCELED) ret = DVD_ERROR_CANCELED;
		else LWP_ThreadSleep(__dvd_wait_queue);
	} while(state!=DVD_STATE_END && state!=DVD_STATE_FATAL_ERROR && state!=DVD_STATE_CANCELED);
	_CPU_ISR_Restore(level);

	return ret;
}

s32 DVD_SpinUpDriveAsync(dvdcmdblk *block,dvdcbcallback cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_SpinUpDriveAsync(%p,%p)\n",block,cb);
#endif
	DVD_Reset(DVD_RESETNONE);

	block->cmd = 0x0011;
	block->cb = cb;
	return __issuecommand(1,block);
}

s32 DVD_SpinUpDrive(dvdcmdblk *block)
{
	s32 ret,state;
	u32 level;
#ifdef _DVD_DEBUG
	printf("DVD_SpinUpDrive(%p)\n",block);
#endif
	ret = DVD_SpinUpDriveAsync(block,__dvd_synccallback);
	if(!ret) return DVD_ERROR_FATAL;

	_CPU_ISR_Disable(level);
	do {
		state = block->state;
		if(state==DVD_STATE_END) ret = DVD_ERROR_OK;
		else if(state==DVD_STATE_FATAL_ERROR) ret = DVD_ERROR_FATAL;
		else if(state==DVD_STATE_CANCELED) ret = DVD_ERROR_CANCELED;
		else LWP_ThreadSleep(__dvd_wait_queue);
	} while(state!=DVD_STATE_END && state!=DVD_STATE_FATAL_ERROR && state!=DVD_STATE_CANCELED);
	_CPU_ISR_Restore(level);

	return ret;
}

s32 DVD_ControlDriveAsync(dvdcmdblk *block,u32 cmd,dvdcbcallback cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_ControlMotorAsync(%p,%d,%p)\n",block,cmd,cb);
#endif
	block->cmd = 0x0012;
	block->cb = cb;
	block->offset = cmd;
	return __issuecommand(1,block);
}

s32 DVD_ControlDrive(dvdcmdblk *block,u32 cmd)
{
	s32 ret,state;
	u32 level;
#ifdef _DVD_DEBUG
	printf("DVD_ControlMotor(%p,%d)\n",block,cmd);
#endif
	ret = DVD_ControlDriveAsync(block,cmd,__dvd_synccallback);
	if(!ret) return DVD_ERROR_FATAL;

	_CPU_ISR_Disable(level);
	do {
		state = block->state;
		if(state==DVD_STATE_END) ret = DVD_ERROR_OK;
		else if(state==DVD_STATE_FATAL_ERROR) ret = DVD_ERROR_FATAL;
		else if(state==DVD_STATE_CANCELED) ret = DVD_ERROR_CANCELED;
		else LWP_ThreadSleep(__dvd_wait_queue);
	} while(state!=DVD_STATE_END && state!=DVD_STATE_FATAL_ERROR && state!=DVD_STATE_CANCELED);
	_CPU_ISR_Restore(level);

	return ret;
}

s32 DVD_SetGCMOffsetAsync(dvdcmdblk *block,s64 offset,dvdcbcallback cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_SetGCMOffsetAsync(%p,%08x,%p)\n",block,offset,cb);
#endif
	block->cmd = 0x0013;
	block->cb = cb;
	block->offset = offset;
	return __issuecommand(1,block);
}

s32 DVD_SetGCMOffset(dvdcmdblk *block,s64 offset)
{
	s32 ret,state;
	u32 level;
#ifdef _DVD_DEBUG
	printf("DVD_SetGCMOffset(%p,%08x)\n",block,offset);
#endif
	ret = DVD_SetGCMOffsetAsync(block,offset,__dvd_synccallback);
	if(!ret) return DVD_ERROR_FATAL;

	_CPU_ISR_Disable(level);
	do {
		state = block->state;
		if(state==DVD_STATE_END) ret = DVD_ERROR_OK;
		else if(state==DVD_STATE_FATAL_ERROR) ret = DVD_ERROR_FATAL;
		else if(state==DVD_STATE_CANCELED) ret = DVD_ERROR_CANCELED;
		else LWP_ThreadSleep(__dvd_wait_queue);
	} while(state!=DVD_STATE_END && state!=DVD_STATE_FATAL_ERROR && state!=DVD_STATE_CANCELED);
	_CPU_ISR_Restore(level);

	return ret;
}

s32 DVD_ReadImmAsyncPrio(dvdcmdblk *block,const dvdcmdbuf cmdbuf,void *buf,u32 len,dvdcbcallback cb,s32 prio)
{
#ifdef _DVD_DEBUG
	printf("DVD_ReadImmAsyncPrio(%p,%p,%p,%d,%p,%d)\n",block,cmdbuf,buf,len,cb,prio);
#endif
	block->cmd = 0x0014;
	block->cmdbuf[0] = cmdbuf[0];
	block->cmdbuf[1] = cmdbuf[1];
	block->cmdbuf[2] = cmdbuf[2];
	block->buf = (void*)buf;
	block->currtxsize = len;
	block->txdsize = 0;
	block->cb = cb;

	return __issuecommand(prio,block);
}

s32 DVD_ReadImmPrio(dvdcmdblk *block,const dvdcmdbuf cmdbuf,void *buf,u32 len,s32 prio)
{
	s32 ret,state;
	u32 level;
#ifdef _DVD_DEBUG
	printf("DVD_ReadImmPrio(%p,%p,%p,%d,%d)\n",block,cmdbuf,buf,len,prio);
#endif
	ret = DVD_ReadImmAsyncPrio(block,cmdbuf,buf,len,__dvd_synccallback,prio);
	if(!ret) return DVD_ERROR_FATAL;

	_CPU_ISR_Disable(level);
	do {
		state = block->state;
		if(state==DVD_STATE_END) ret = block->txdsize;
		else if(state==DVD_STATE_FATAL_ERROR) ret = DVD_ERROR_FATAL;
		else if(state==DVD_STATE_CANCELED) ret = DVD_ERROR_CANCELED;
		else LWP_ThreadSleep(__dvd_wait_queue);
	} while(state!=DVD_STATE_END && state!=DVD_STATE_FATAL_ERROR && state!=DVD_STATE_CANCELED);
	_CPU_ISR_Restore(level);

	return ret;
}

s32 DVD_WriteImmAsyncPrio(dvdcmdblk *block,const dvdcmdbuf cmdbuf,const void *buf,u32 len,dvdcbcallback cb,s32 prio)
{
#ifdef _DVD_DEBUG
	printf("DVD_WriteImmAsyncPrio(%p,%p,%p,%d,%p,%d)\n",block,cmdbuf,buf,len,cb,prio);
#endif
	block->cmd = 0x0015;
	block->cmdbuf[0] = cmdbuf[0];
	block->cmdbuf[1] = cmdbuf[1];
	block->cmdbuf[2] = cmdbuf[2];
	block->immbuf = __lswx(buf,len);
	block->currtxsize = len;
	block->txdsize = 0;
	block->cb = cb;

	return __issuecommand(prio,block);
}

s32 DVD_WriteImmPrio(dvdcmdblk *block,const dvdcmdbuf cmdbuf,const void *buf,u32 len,s32 prio)
{
	s32 ret,state;
	u32 level;
#ifdef _DVD_DEBUG
	printf("DVD_WriteImmPrio(%p,%p,%p,%d,%d)\n",block,cmdbuf,buf,len,prio);
#endif
	ret = DVD_WriteImmAsyncPrio(block,cmdbuf,buf,len,__dvd_synccallback,prio);
	if(!ret) return DVD_ERROR_FATAL;

	_CPU_ISR_Disable(level);
	do {
		state = block->state;
		if(state==DVD_STATE_END) ret = block->txdsize;
		else if(state==DVD_STATE_FATAL_ERROR) ret = DVD_ERROR_FATAL;
		else if(state==DVD_STATE_CANCELED) ret = DVD_ERROR_CANCELED;
		else LWP_ThreadSleep(__dvd_wait_queue);
	} while(state!=DVD_STATE_END && state!=DVD_STATE_FATAL_ERROR && state!=DVD_STATE_CANCELED);
	_CPU_ISR_Restore(level);

	return ret;
}

s32 DVD_ReadDmaAsyncPrio(dvdcmdblk *block,const dvdcmdbuf cmdbuf,void *buf,u32 len,dvdcbcallback cb,s32 prio)
{
#ifdef _DVD_DEBUG
	printf("DVD_ReadDmaAsyncPrio(%p,%p,%p,%d,%p,%d)\n",block,cmdbuf,buf,len,cb,prio);
#endif
	block->cmd = 0x0016;
	block->cmdbuf[0] = cmdbuf[0];
	block->cmdbuf[1] = cmdbuf[1];
	block->cmdbuf[2] = cmdbuf[2];
	block->buf = (void*)buf;
	block->currtxsize = len;
	block->txdsize = 0;
	block->cb = cb;

	return __issuecommand(prio,block);
}

s32 DVD_ReadDmaPrio(dvdcmdblk *block,const dvdcmdbuf cmdbuf,void *buf,u32 len,s32 prio)
{
	s32 ret,state;
	u32 level;
#ifdef _DVD_DEBUG
	printf("DVD_ReadDmaPrio(%p,%p,%p,%d,%d)\n",block,cmdbuf,buf,len,prio);
#endif
	ret = DVD_ReadDmaAsyncPrio(block,cmdbuf,buf,len,__dvd_synccallback,prio);
	if(!ret) return DVD_ERROR_FATAL;

	_CPU_ISR_Disable(level);
	do {
		state = block->state;
		if(state==DVD_STATE_END) ret = block->txdsize;
		else if(state==DVD_STATE_FATAL_ERROR) ret = DVD_ERROR_FATAL;
		else if(state==DVD_STATE_CANCELED) ret = DVD_ERROR_CANCELED;
		else LWP_ThreadSleep(__dvd_wait_queue);
	} while(state!=DVD_STATE_END && state!=DVD_STATE_FATAL_ERROR && state!=DVD_STATE_CANCELED);
	_CPU_ISR_Restore(level);

	return ret;
}

s32 DVD_WriteDmaAsyncPrio(dvdcmdblk *block,const dvdcmdbuf cmdbuf,const void *buf,u32 len,dvdcbcallback cb,s32 prio)
{
#ifdef _DVD_DEBUG
	printf("DVD_WriteDmaAsyncPrio(%p,%p,%p,%d,%p,%d)\n",block,cmdbuf,buf,len,cb,prio);
#endif
	block->cmd = 0x0017;
	block->cmdbuf[0] = cmdbuf[0];
	block->cmdbuf[1] = cmdbuf[1];
	block->cmdbuf[2] = cmdbuf[2];
	block->buf = (void*)buf;
	block->currtxsize = len;
	block->txdsize = 0;
	block->cb = cb;

	return __issuecommand(prio,block);
}

s32 DVD_WriteDmaPrio(dvdcmdblk *block,const dvdcmdbuf cmdbuf,const void *buf,u32 len,s32 prio)
{
	s32 ret,state;
	u32 level;
#ifdef _DVD_DEBUG
	printf("DVD_WriteDmaPrio(%p,%p,%p,%d,%d)\n",block,cmdbuf,buf,len,prio);
#endif
	ret = DVD_WriteDmaAsyncPrio(block,cmdbuf,buf,len,__dvd_synccallback,prio);
	if(!ret) return DVD_ERROR_FATAL;

	_CPU_ISR_Disable(level);
	do {
		state = block->state;
		if(state==DVD_STATE_END) ret = block->txdsize;
		else if(state==DVD_STATE_FATAL_ERROR) ret = DVD_ERROR_FATAL;
		else if(state==DVD_STATE_CANCELED) ret = DVD_ERROR_CANCELED;
		else LWP_ThreadSleep(__dvd_wait_queue);
	} while(state!=DVD_STATE_END && state!=DVD_STATE_FATAL_ERROR && state!=DVD_STATE_CANCELED);
	_CPU_ISR_Restore(level);

	return ret;
}

s32 DVD_GcodeReadAsync(dvdcmdblk *block,void *buf,u32 len,u32 offset,dvdcbcallback cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_GcodeReadAsync(%p,%p,%d,%d,%p)\n",block,buf,len,offset,cb);
#endif
	block->cmd = 0x0018;
	block->buf = (void*)buf;
	block->len = len;
	block->offset = offset;
	block->txdsize = 0;
	block->cb = cb;

	return __issuecommand(2,block);
}

s32 DVD_GcodeRead(dvdcmdblk *block,void *buf,u32 len,u32 offset)
{
	s32 ret,state;
	u32 level;
#ifdef _DVD_DEBUG
	printf("DVD_GcodeRead(%p,%p,%d,%d)\n",block,buf,len,offset);
#endif
	ret = DVD_GcodeReadAsync(block,buf,len,offset,__dvd_synccallback);
	if(!ret) return DVD_ERROR_FATAL;

	_CPU_ISR_Disable(level);
	do {
		state = block->state;
		if(state==DVD_STATE_END) ret = block->txdsize;
		else if(state==DVD_STATE_FATAL_ERROR) ret = DVD_ERROR_FATAL;
		else if(state==DVD_STATE_CANCELED) ret = DVD_ERROR_CANCELED;
		else LWP_ThreadSleep(__dvd_wait_queue);
	} while(state!=DVD_STATE_END && state!=DVD_STATE_FATAL_ERROR && state!=DVD_STATE_CANCELED);
	_CPU_ISR_Restore(level);

	return ret;
}

s32 DVD_GcodeWriteAsync(dvdcmdblk *block,const void *buf,u32 len,u32 offset,dvdcbcallback cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_GcodeWriteAsync(%p,%p,%d,%d,%p)\n",block,buf,len,offset,cb);
#endif
	block->cmd = 0x0019;
	block->buf = (void*)buf;
	block->len = len;
	block->offset = offset;
	block->txdsize = 0;
	block->cb = cb;

	return __issuecommand(2,block);
}

s32 DVD_GcodeWrite(dvdcmdblk *block,const void *buf,u32 len,u32 offset)
{
	s32 ret,state;
	u32 level;
#ifdef _DVD_DEBUG
	printf("DVD_GcodeWrite(%p,%p,%d,%d)\n",block,buf,len,offset);
#endif
	ret = DVD_GcodeWriteAsync(block,buf,len,offset,__dvd_synccallback);
	if(!ret) return DVD_ERROR_FATAL;

	_CPU_ISR_Disable(level);
	do {
		state = block->state;
		if(state==DVD_STATE_END) ret = block->txdsize;
		else if(state==DVD_STATE_FATAL_ERROR) ret = DVD_ERROR_FATAL;
		else if(state==DVD_STATE_CANCELED) ret = DVD_ERROR_CANCELED;
		else LWP_ThreadSleep(__dvd_wait_queue);
	} while(state!=DVD_STATE_END && state!=DVD_STATE_FATAL_ERROR && state!=DVD_STATE_CANCELED);
	_CPU_ISR_Restore(level);

	return ret;
}

s32 DVD_GetCmdBlockStatus(const dvdcmdblk *block)
{
	s32 ret;
	u32 level;

	_CPU_ISR_Disable(level);
	if((ret=block->state)==DVD_STATE_COVER_CLOSED) ret = DVD_STATE_BUSY;
	_CPU_ISR_Restore(level);
	return ret;
}

s32 DVD_GetDriveStatus(void)
{
	s32 ret;
	u32 level;

	_CPU_ISR_Disable(level);
	if(__dvd_fatalerror) ret = DVD_STATE_FATAL_ERROR;
	else {
		if(__dvd_pausingflag) ret = DVD_STATE_PAUSING;
		else {
			if(!__dvd_executing || __dvd_executing==&__dvd_dummycmdblk) ret = DVD_STATE_END;
			else ret = DVD_GetCmdBlockStatus(__dvd_executing);
		}
	}
	_CPU_ISR_Restore(level);
	return ret;
}

void DVD_Pause(void)
{
	u32 level;

	_CPU_ISR_Disable(level);
	__dvd_pauseflag = 1;
	if(!__dvd_executing) __dvd_pausingflag = 1;
	_CPU_ISR_Restore(level);
}

void DVD_Resume(void)
{
	u32 level;

	_CPU_ISR_Disable(level);
	__dvd_pauseflag = 0;
	if(__dvd_pausingflag) {
		__dvd_pausingflag = 0;
		__dvd_stateready();
	}
	_CPU_ISR_Restore(level);
}

void DVD_Reset(u32 reset_mode)
{
#ifdef _DVD_DEBUG
	printf("DVD_Reset(%d)\n",reset_mode);
#endif
	__dvd_drivestate &= ~(DVD_INTEROPER|DVD_CHIPPRESENT|DVD_DRIVERESET);

	if(reset_mode!=DVD_RESETNONE)
		DVD_LowReset(reset_mode);

	_diReg[0] = (DVD_DE_MSK|DVD_TC_MSK|DVD_BRK_MSK);
	_diReg[1] = _diReg[1];

	__dvd_resetrequired = 0;
	__dvd_internalretries = 0;
}

u32 DVD_ResetRequired(void)
{
	return __dvd_resetrequired;
}

static void callback(s32 result,dvdcmdblk *block)
{
#ifdef _DVD_DEBUG
	printf("callback(%d)\n",result);
#endif
	if(result==DVD_ERROR_OK) {
		DVD_ReadDiskID(block,&__dvd_tmpid0,callback);
		return;
	}
	else if(result>=DVD_DISKIDSIZE) {
		memcpy(__dvd_diskID,&__dvd_tmpid0,DVD_DISKIDSIZE);
	} else if(result==DVD_ERROR_COVER_CLOSED) {
		DVD_SpinUpDriveAsync(block,callback);
		return;
	}
	if(__dvd_mountusrcb) __dvd_mountusrcb(result,block);
}

s32 DVD_MountAsync(dvdcmdblk *block,dvdcbcallback cb)
{
#ifdef _DVD_DEBUG
	printf("DVD_MountAsync()\n");
#endif
	__dvd_mountusrcb = cb;
	DVD_Reset(DVD_RESETHARD);
	udelay(1150*1000);
	return DVD_SpinUpDriveAsync(block,callback);
}

s32 DVD_Mount(void)
{
	s32 ret,state;
	u32 level;
#ifdef _DVD_DEBUG
	printf("DVD_Mount()\n");
#endif
	ret = DVD_MountAsync(&__dvd_block$15,__dvd_synccallback);
	if(!ret) return DVD_ERROR_FATAL;

	_CPU_ISR_Disable(level);
	do {
		state = __dvd_block$15.state;
		if(state==DVD_STATE_END) ret = DVD_ERROR_OK;
		else if(state==DVD_STATE_FATAL_ERROR) ret = DVD_ERROR_FATAL;
		else if(state==DVD_STATE_CANCELED) ret = DVD_ERROR_CANCELED;
		else LWP_ThreadSleep(__dvd_wait_queue);
	} while(state!=DVD_STATE_END && state!=DVD_STATE_FATAL_ERROR && state!=DVD_STATE_CANCELED);
	__dvd_mountusrcb = NULL;		//set to zero coz this is only used to sync for this function.
	_CPU_ISR_Restore(level);

	return ret;
}

dvddiskid* DVD_GetCurrentDiskID(void)
{
	return __dvd_diskID;
}

dvddrvinfo* DVD_GetDriveInfo(void)
{
	return &__dvd_driveinfo;
}

void DVD_Init(void)
{
	u32 i;
#ifdef _DVD_DEBUG
	printf("DVD_Init()\n");
#endif
	if(!__dvd_initflag) {
		__dvd_initflag = 1;
		__dvd_clearwaitingqueue();
		__DVDInitWA();

		IRQ_Request(IRQ_PI_DI,__DVDInterruptHandler);
		__UnmaskIrq(IRQMASK(IRQ_PI_DI));

		SYS_CreateAlarm(&__dvd_timeoutalarm);
		LWP_InitQueue(&__dvd_wait_queue);

		for(i=0;i<9;i++) _diReg[i] = 0;

		_piReg[9] |= 0x0005;
		__dvd_resetoccured = 1;
		__dvd_lastresetend = __SYS_GetSystemTime();

		_diReg[0] = (DVD_DE_MSK|DVD_TC_MSK|DVD_BRK_MSK);
		_diReg[1] = DVD_CVR_MSK;
	}
}

u32 DVD_SetAutoInvalidation(u32 auto_inv)
{
	u32 ret = __dvd_autoinvalidation;
	__dvd_autoinvalidation= auto_inv;
	return ret;
}

static bool __gcdvd_Startup(DISC_INTERFACE *disc)
{
	DVD_Init();

	if (mfpvr() == 0x00083214) // GameCube
	{
		DVD_Mount();
	}
	else
	{
		DVD_Reset(DVD_RESETHARD);
		DVD_ReadDiskID(&__dvd_block$15, &__dvd_tmpid0, callback);
	}
	return true;
}

static bool __gcdvd_IsInserted(DISC_INTERFACE *disc)
{
	u32 status = 0;
	DVD_LowGetStatus(&status, NULL);

	if(DVD_STATUS(status) == DVD_STATUS_READY) 
		return true;

	return false;
}

static bool __gcdvd_ReadSectors(DISC_INTERFACE *disc,sec_t sector,sec_t numSectors,void *buffer)
{
	dvdcmdblk blk;

	if(disc->ioType != DEVICE_TYPE_GAMECUBE_DVD) return false;
	if(!(disc->features & FEATURE_MEDIUM_CANREAD)) return false;
	if(sector & ~0x7fffff) return false;
	if(numSectors & ~0x1fffff) return false;
	if(disc->bytesPerSector != 2048) return false;
	if(!SYS_IsDMAAddress(buffer, 32)) return false;

	if(DVD_ReadAbs(&blk, buffer, numSectors << 11, sector << 11) < 0)
		return false;

	return true;
}

static bool __gcdvd_WriteSectors(DISC_INTERFACE *disc,sec_t sector,sec_t numSectors,const void *buffer)
{
	return false;
}

static bool __gcdvd_ClearStatus(DISC_INTERFACE *disc)
{
	return true;
}

static bool __gcdvd_Shutdown(DISC_INTERFACE *disc)
{
	dvdcmdblk blk;
	DVD_StopMotor(&blk);
	return true;
}

static bool __gcode_Startup(DISC_INTERFACE *disc)
{
	dvdcmdblk blk;

	DVD_Init();

	if(DVD_Inquiry(&blk, &__dvd_driveinfo) < 0)
		return false;

	if(__dvd_driveinfo.rel_date != 0x20196c64)
		return false;

	if(__dvd_driveinfo.pad[1] == 'w')
		disc->features |= FEATURE_MEDIUM_CANWRITE;
	else
		disc->features &= ~FEATURE_MEDIUM_CANWRITE;

	__dvd_gcode_writebufsize = __dvd_driveinfo.pad[3] + 1;

	return true;
}

static bool __gcode_IsInserted(DISC_INTERFACE *disc)
{
	if(DVD_LowGetCoverStatus() == 1)
		return false;

	return true;
}

static bool __gcode_ReadSectors(DISC_INTERFACE *disc,sec_t sector,sec_t numSectors,void *buffer)
{
	dvdcmdblk blk;

	if(disc->ioType != DEVICE_TYPE_GAMECUBE_GCODE) return false;
	if(!(disc->features & FEATURE_MEDIUM_CANREAD)) return false;
	if((u32)sector != sector) return false;
	if(numSectors & ~0x7fffff) return false;
	if(disc->bytesPerSector != 512) return false;
	if(!SYS_IsDMAAddress(buffer, 32)) return false;

	if(DVD_GcodeRead(&blk, buffer, numSectors << 9, sector) < 0)
		return false;

	return true;
}

static bool __gcode_WriteSectors(DISC_INTERFACE *disc,sec_t sector,sec_t numSectors,const void *buffer)
{
	dvdcmdblk blk;

	if(disc->ioType != DEVICE_TYPE_GAMECUBE_GCODE) return false;
	if(!(disc->features & FEATURE_MEDIUM_CANWRITE)) return false;
	if((u32)sector != sector) return false;
	if((u32)numSectors != numSectors) return false;
	if(disc->bytesPerSector != 512) return false;
	if(!SYS_IsDMAAddress(buffer, 32)) return false;

	if(DVD_GcodeWrite(&blk, buffer, numSectors, sector) < 0)
		return false;

	return true;
}

static bool __gcode_ClearStatus(DISC_INTERFACE *disc)
{
	return true;
}

static bool __gcode_Shutdown(DISC_INTERFACE *disc)
{
	return true;
}

DISC_INTERFACE __io_gcdvd = {
	DEVICE_TYPE_GAMECUBE_DVD,
	FEATURE_MEDIUM_CANREAD | FEATURE_GAMECUBE_DVD,
	__gcdvd_Startup,
	__gcdvd_IsInserted,
	__gcdvd_ReadSectors,
	__gcdvd_WriteSectors,
	__gcdvd_ClearStatus,
	__gcdvd_Shutdown,
	0x800000,
	2048
};

DISC_INTERFACE __io_gcode = {
	DEVICE_TYPE_GAMECUBE_GCODE,
	FEATURE_MEDIUM_CANREAD | FEATURE_GAMECUBE_DVD,
	__gcode_Startup,
	__gcode_IsInserted,
	__gcode_ReadSectors,
	__gcode_WriteSectors,
	__gcode_ClearStatus,
	__gcode_Shutdown,
	0x100000000,
	512
};
