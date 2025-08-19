#ifndef __OGC_DVDLOW_H__
#define __OGC_DVDLOW_H__

#include <gctypes.h>
#include "dvd.h"

#define DVD_COVER_RESET		0
#define DVD_COVER_OPEN		1
#define DVD_COVER_CLOSED	2

#ifdef __cplusplus
   extern "C" {
#endif /* __cplusplus */

typedef void (*dvdcallbacklow)(s32 result);

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
s32 DVD_LowGcodeRead(void *buf,u32 len,u32 offset,dvdcallbacklow cb);
s32 DVD_LowGcodeWriteBuffer(void *buf,u32 len,dvdcallbacklow cb);
s32 DVD_LowGcodeWrite(u32 len,u32 offset,dvdcallbacklow cb);

#ifdef __cplusplus
   }
#endif /* __cplusplus */

#endif
