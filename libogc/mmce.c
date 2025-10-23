/*-------------------------------------------------------------

mmce.c -- MMCE support

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

#include <gctypes.h>
#include <ogc/cache.h>
#include <ogc/card.h>
#include <ogc/disc_io.h>
#include <ogc/dvd.h>
#include <ogc/exi.h>
#include <ogc/irq.h>
#include <ogc/lwp.h>
#include <ogc/mmce.h>
#include <ogc/system.h>
#include <string.h>

typedef struct {
	s32 result;
	bool attached;
	u8 cmd[3];
	u32 cmdLen;
	u32 mode;
	u32 repeat;
	void *buf;
	u32 bufLen;
	EXICallback exiCallback;
	lwpq_t syncQueue;
} MMCEControlBlock;

static MMCEControlBlock __MMCE[EXI_CHANNEL_MAX];

static s32 ExiHandler(s32 chan, s32 dev);
static s32 TxHandler(s32 chan, s32 dev);

static vu32 *const _piReg = (u32 *)0xCC003000;

static void __attribute__((constructor)) __MMCE_Init(void)
{
	LWP_InitQueue(&__MMCE[0].syncQueue);
	LWP_InitQueue(&__MMCE[1].syncQueue);
	LWP_InitQueue(&__MMCE[2].syncQueue);
}

static s32 MMCE_Sync(s32 chan)
{
	s32 result;
	u32 level = IRQ_Disable();

	while ((result = __MMCE[chan].result) == MMCE_RESULT_BUSY)
		if (LWP_ThreadSleep(__MMCE[chan].syncQueue)) break;

	IRQ_Restore(level);
	return result;
}

static bool IsValidType(u32 type)
{
	switch (type) {
		case EXI_MEMCARD59:
		case EXI_MEMCARD123:
		case EXI_MEMCARD251:
		case EXI_MEMCARD507:
		case EXI_MEMCARD1019:
		case EXI_MEMCARD2043:
		case EXI_MEMCARDPRO:
			return true;
		default:
			return false;
	}
}

s32 MMCE_ProbeEx(s32 chan)
{
	return CARD_ProbeEx(chan, NULL, NULL);
}

s32 MMCE_GetDeviceID(s32 chan, u32 *id)
{
	bool err = false;
	u8 cmd[2];

	if (!EXI_LockEx(chan, EXI_DEVICE_0)) return MMCE_RESULT_BUSY;
	if (!EXI_Select(chan, EXI_DEVICE_0, EXI_SPEED16MHZ)) {
		EXI_Unlock(chan);
		return MMCE_RESULT_NOCARD;
	}

	cmd[0] = 0x8B;
	cmd[1] = 0x00;

	err |= !EXI_ImmEx(chan, cmd, sizeof(cmd), EXI_WRITE);
	err |= !EXI_ImmEx(chan, id, sizeof(*id), EXI_READ);
	err |= !EXI_Deselect(chan);
	EXI_Unlock(chan);

	if (err)
		return MMCE_RESULT_NOCARD;
	else if ((*id & ~0xFFFF) != EXI_MEMCARDPRO)
		return MMCE_RESULT_WRONGDEVICE;
	else
		return MMCE_RESULT_READY;
}

s32 MMCE_GetAccessMode(s32 chan, u8 *mode)
{
	bool err = false;
	u8 cmd[3];

	if (!EXI_LockEx(chan, EXI_DEVICE_0)) return MMCE_RESULT_BUSY;
	if (!EXI_Select(chan, EXI_DEVICE_0, EXI_SPEED16MHZ)) {
		EXI_Unlock(chan);
		return MMCE_RESULT_NOCARD;
	}

	cmd[0] = 0x8B;
	cmd[1] = 0x01;
	cmd[2] = 0x00;

	err |= !EXI_ImmEx(chan, cmd, sizeof(cmd), EXI_WRITE);
	err |= !EXI_ImmEx(chan, mode, sizeof(*mode), EXI_READ);
	err |= !EXI_Deselect(chan);
	EXI_Unlock(chan);

	return err ? MMCE_RESULT_NOCARD : MMCE_RESULT_READY;
}

s32 MMCE_SetAccessMode(s32 chan, u8 mode)
{
	bool err = false;
	u8 cmd[3];

	if (!EXI_LockEx(chan, EXI_DEVICE_0)) return MMCE_RESULT_BUSY;
	if (!EXI_Select(chan, EXI_DEVICE_0, EXI_SPEED16MHZ)) {
		EXI_Unlock(chan);
		return MMCE_RESULT_NOCARD;
	}

	cmd[0] = 0x8B;
	cmd[1] = 0x02;
	cmd[2] = mode;

	err |= !EXI_ImmEx(chan, cmd, sizeof(cmd), EXI_WRITE);
	err |= !EXI_Deselect(chan);

	if (!err) {
		MMCEControlBlock *cb = &__MMCE[chan];
		cb->result = MMCE_RESULT_BUSY;
		cb->repeat = 0;

		if (chan < EXI_CHANNEL_2)
			cb->exiCallback = EXI_RegisterEXICallback(chan, ExiHandler);
	}

	EXI_Unlock(chan);

	return err ? MMCE_RESULT_NOCARD : MMCE_Sync(chan);
}

s32 MMCE_SetDiskID(s32 chan, const dvddiskid *diskID)
{
	static const char digits[16] = "0123456789ABCDEF";
	bool err = false;
	u8 cmd[12];

	if (!EXI_LockEx(chan, EXI_DEVICE_0)) return MMCE_RESULT_BUSY;
	if (!EXI_Select(chan, EXI_DEVICE_0, EXI_SPEED16MHZ)) {
		EXI_Unlock(chan);
		return MMCE_RESULT_NOCARD;
	}

	memset(cmd, 0, sizeof(cmd));
	cmd[0] = 0x8B;
	cmd[1] = 0x11;

	if (diskID) {
		memcpy(&cmd[2], diskID->gamename, 4);
		memcpy(&cmd[6], diskID->company,  2);
		cmd[8]  = digits[diskID->disknum / 16];
		cmd[9]  = digits[diskID->disknum % 16];
		cmd[10] = digits[diskID->gamever / 16];
		cmd[11] = digits[diskID->gamever % 16];
	}

	err |= !EXI_ImmEx(chan, cmd, sizeof(cmd), EXI_WRITE);
	err |= !EXI_Deselect(chan);
	EXI_Unlock(chan);

	return err ? MMCE_RESULT_NOCARD : MMCE_RESULT_READY;
}

s32 MMCE_GetDiskInfo(s32 chan, char diskInfo[64])
{
	bool err = false;
	u8 cmd[3];

	if (!EXI_LockEx(chan, EXI_DEVICE_0)) return MMCE_RESULT_BUSY;
	if (!EXI_Select(chan, EXI_DEVICE_0, EXI_SPEED16MHZ)) {
		EXI_Unlock(chan);
		return MMCE_RESULT_NOCARD;
	}

	cmd[0] = 0x8B;
	cmd[1] = 0x12;
	cmd[2] = 0x00;

	err |= !EXI_ImmEx(chan, cmd, sizeof(cmd), EXI_WRITE);
	err |= !EXI_ImmEx(chan, diskInfo, 64, EXI_READ);
	err |= !EXI_Deselect(chan);
	EXI_Unlock(chan);

	return err ? MMCE_RESULT_NOCARD : MMCE_RESULT_READY;
}

s32 MMCE_SetDiskInfo(s32 chan, const char diskInfo[64])
{
	bool err = false;
	u8 cmd[2];

	if (!EXI_LockEx(chan, EXI_DEVICE_0)) return MMCE_RESULT_BUSY;
	if (!EXI_Select(chan, EXI_DEVICE_0, EXI_SPEED16MHZ)) {
		EXI_Unlock(chan);
		return MMCE_RESULT_NOCARD;
	}

	cmd[0] = 0x8B;
	cmd[1] = 0x13;

	err |= !EXI_ImmEx(chan, cmd, sizeof(cmd), EXI_WRITE);
	err |= !EXI_ImmEx(chan, (char *)diskInfo, 64, EXI_WRITE);
	err |= !EXI_Deselect(chan);
	EXI_Unlock(chan);

	return err ? MMCE_RESULT_NOCARD : MMCE_RESULT_READY;
}

s32 MMCE_GetGameID(s32 chan, char gameID[10])
{
	bool err = false;
	u8 cmd[3];

	if (!EXI_LockEx(chan, EXI_DEVICE_0)) return MMCE_RESULT_BUSY;
	if (!EXI_Select(chan, EXI_DEVICE_0, EXI_SPEED16MHZ)) {
		EXI_Unlock(chan);
		return MMCE_RESULT_NOCARD;
	}

	cmd[0] = 0x8B;
	cmd[1] = 0x1C;
	cmd[2] = 0x00;

	err |= !EXI_ImmEx(chan, cmd, sizeof(cmd), EXI_WRITE);
	err |= !EXI_ImmEx(chan, gameID, 10, EXI_READ);
	err |= !EXI_Deselect(chan);
	EXI_Unlock(chan);

	return err ? MMCE_RESULT_NOCARD : MMCE_RESULT_READY;
}

s32 MMCE_SetGameID(s32 chan, const char gameID[10])
{
	bool err = false;
	u8 cmd[2];

	if (!EXI_LockEx(chan, EXI_DEVICE_0)) return MMCE_RESULT_BUSY;
	if (!EXI_Select(chan, EXI_DEVICE_0, EXI_SPEED16MHZ)) {
		EXI_Unlock(chan);
		return MMCE_RESULT_NOCARD;
	}

	cmd[0] = 0x8B;
	cmd[1] = 0x1D;

	err |= !EXI_ImmEx(chan, cmd, sizeof(cmd), EXI_WRITE);
	err |= !EXI_ImmEx(chan, (char *)gameID, 10, EXI_WRITE);
	err |= !EXI_Deselect(chan);
	EXI_Unlock(chan);

	return err ? MMCE_RESULT_NOCARD : MMCE_RESULT_READY;
}

static s32 ExiHandler(s32 chan, s32 dev)
{
	MMCEControlBlock *cb = &__MMCE[chan];

	if (!cb->repeat) {
		cb->result = MMCE_RESULT_READY;
		LWP_ThreadBroadcast(cb->syncQueue);

		if (chan < EXI_CHANNEL_2)
			EXI_RegisterEXICallback(chan, cb->exiCallback);
	} else {
		if (!EXI_Lock(chan, EXI_DEVICE_0, ExiHandler))
			return FALSE;

		if (EXI_Select(chan, EXI_DEVICE_0, EXI_SPEED32MHZ)) {
			EXI_ImmEx(chan, cb->cmd, cb->cmdLen, EXI_WRITE);
			EXI_Dma(chan, cb->buf, cb->bufLen, cb->mode, TxHandler);
		}
	}

	return TRUE;
}

static s32 TxHandler(s32 chan, s32 dev)
{
	bool err = false;

	err |= !EXI_Deselect(chan);
	EXI_Unlock(chan);

	if (!err) {
		MMCEControlBlock *cb = &__MMCE[chan];
		cb->buf += cb->bufLen;

		if (!--cb->repeat) {
			cb->result = MMCE_RESULT_READY;
			LWP_ThreadBroadcast(cb->syncQueue);

			if (chan < EXI_CHANNEL_2)
				EXI_RegisterEXICallback(chan, cb->exiCallback);
		}
	}

	return TRUE;
}

s32 MMCE_ReadSectors(s32 chan, u32 sector, u16 numSectors, void *buffer)
{
	bool err = false;
	u8 cmd[8];

	if (!EXI_LockEx(chan, EXI_DEVICE_0)) return MMCE_RESULT_BUSY;
	if (!EXI_Select(chan, EXI_DEVICE_0, EXI_SPEED32MHZ)) {
		EXI_Unlock(chan);
		return MMCE_RESULT_NOCARD;
	}

	cmd[0] = 0x8B;
	cmd[1] = 0x20;
	cmd[2] = sector >> 24;
	cmd[3] = sector >> 16;
	cmd[4] = sector >> 8;
	cmd[5] = sector;
	cmd[6] = numSectors >> 8;
	cmd[7] = numSectors;

	err |= !EXI_ImmEx(chan, cmd, sizeof(cmd), EXI_WRITE);
	err |= !EXI_Deselect(chan);

	if (!err) {
		MMCEControlBlock *cb = &__MMCE[chan];
		cb->result = MMCE_RESULT_BUSY;
		cb->cmd[0] = 0x8B;
		cb->cmd[1] = 0x21;
		cb->cmd[2] = 0x00;
		cb->cmdLen = 3;
		cb->mode = EXI_READ;
		cb->repeat = numSectors;
		cb->buf = buffer;
		cb->bufLen = 512;
		DCInvalidateRange(cb->buf, cb->bufLen * cb->repeat);

		if (chan < EXI_CHANNEL_2)
			cb->exiCallback = EXI_RegisterEXICallback(chan, ExiHandler);
	}

	EXI_Unlock(chan);

	return err ? MMCE_RESULT_NOCARD : MMCE_Sync(chan);
}

s32 MMCE_WriteSectors(s32 chan, u32 sector, u16 numSectors, const void *buffer)
{
	bool err = false;
	u8 cmd[8];

	if (!EXI_LockEx(chan, EXI_DEVICE_0)) return MMCE_RESULT_BUSY;
	if (!EXI_Select(chan, EXI_DEVICE_0, EXI_SPEED32MHZ)) {
		EXI_Unlock(chan);
		return MMCE_RESULT_NOCARD;
	}

	cmd[0] = 0x8B;
	cmd[1] = 0x22;
	cmd[2] = sector >> 24;
	cmd[3] = sector >> 16;
	cmd[4] = sector >> 8;
	cmd[5] = sector;
	cmd[6] = numSectors >> 8;
	cmd[7] = numSectors;

	err |= !EXI_ImmEx(chan, cmd, sizeof(cmd), EXI_WRITE);
	err |= !EXI_Deselect(chan);

	if (!err) {
		MMCEControlBlock *cb = &__MMCE[chan];
		cb->result = MMCE_RESULT_BUSY;
		cb->cmd[0] = 0x8B;
		cb->cmd[1] = 0x23;
		cb->cmdLen = 2;
		cb->mode = EXI_WRITE;
		cb->repeat = numSectors;
		cb->buf = (void *)buffer;
		cb->bufLen = 512;
		DCStoreRange(cb->buf, cb->bufLen * cb->repeat);

		if (chan < EXI_CHANNEL_2)
			cb->exiCallback = EXI_RegisterEXICallback(chan, ExiHandler);
	}

	EXI_Unlock(chan);

	return err ? MMCE_RESULT_NOCARD : MMCE_Sync(chan);
}

static s32 ExtHandler(s32 chan, s32 dev)
{
	MMCEControlBlock *cb = &__MMCE[chan];

	EXI_RegisterEXICallback(chan, NULL);
	cb->attached = false;
	cb->result = MMCE_RESULT_NOCARD;
	LWP_ThreadBroadcast(cb->syncQueue);

	return TRUE;
}

static void DbgHandler(u32 irq, frame_context *ctx)
{
	_piReg[0] = 1 << 12;
	ExiHandler(EXI_CHANNEL_2, EXI_DEVICE_0);
}

static bool __mmce_startup(DISC_INTERFACE *disc)
{
	s32 chan = (disc->ioType & 0xFF) - '0';
	u32 type, id;
	u8 mode;

	if (disc->ioType < DEVICE_TYPE_GAMECUBE_MMCE(0)) return false;
	if (disc->ioType > DEVICE_TYPE_GAMECUBE_MMCE(2)) return false;
	if (__MMCE[chan].attached) return true;

	while (!EXI_ProbeEx(chan));

	if (!EXI_GetType(chan, EXI_DEVICE_0, &type) || !IsValidType(type))
		return false;
	if (chan < EXI_CHANNEL_2)
		if (!EXI_Attach(chan, ExtHandler))
			return false;

	if (MMCE_GetDeviceID(chan, &id) < 0 || (id & 0xFFFF) < 0x0101) {
		if (chan < EXI_CHANNEL_2)
			EXI_Detach(chan);
		return false;
	}

	__MMCE[chan].attached = true;

	if (chan == EXI_CHANNEL_2) {
		IRQ_Request(IRQ_PI_DEBUG, DbgHandler);
		__UnmaskIrq(IM_PI_DEBUG);
	}

	MMCE_SetAccessMode(chan, MMCE_MODE_READWRITE);

	if (MMCE_GetAccessMode(chan, &mode) == MMCE_RESULT_READY && mode == MMCE_MODE_READWRITE)
		disc->features |= FEATURE_MEDIUM_CANWRITE;
	else
		disc->features &= ~FEATURE_MEDIUM_CANWRITE;

	return true;
}

static bool __mmce_isInserted(DISC_INTERFACE *disc)
{
	s32 chan = (disc->ioType & 0xFF) - '0';

	if (disc->ioType < DEVICE_TYPE_GAMECUBE_MMCE(0)) return false;
	if (disc->ioType > DEVICE_TYPE_GAMECUBE_MMCE(2)) return false;

	return __MMCE[chan].attached;
}

static bool __mmce_readSectors(DISC_INTERFACE *disc, sec_t sector, sec_t numSectors, void *buffer)
{
	s32 chan = (disc->ioType & 0xFF) - '0';

	if (disc->ioType < DEVICE_TYPE_GAMECUBE_MMCE(0)) return false;
	if (disc->ioType > DEVICE_TYPE_GAMECUBE_MMCE(2)) return false;
	if (!(disc->features & FEATURE_MEDIUM_CANREAD)) return false;
	if ((u32)sector != sector) return false;
	if ((u32)numSectors != numSectors) return false;
	if (disc->bytesPerSector != 512) return false;
	if (!SYS_IsDMAAddress(buffer, 32)) return false;
	if (!__MMCE[chan].attached) return false;

	return MMCE_ReadSectors(chan, sector, numSectors, buffer) == MMCE_RESULT_READY;
}

static bool __mmce_writeSectors(DISC_INTERFACE *disc, sec_t sector, sec_t numSectors, const void *buffer)
{
	s32 chan = (disc->ioType & 0xFF) - '0';

	if (disc->ioType < DEVICE_TYPE_GAMECUBE_MMCE(0)) return false;
	if (disc->ioType > DEVICE_TYPE_GAMECUBE_MMCE(2)) return false;
	if (!(disc->features & FEATURE_MEDIUM_CANWRITE)) return false;
	if ((u32)sector != sector) return false;
	if ((u32)numSectors != numSectors) return false;
	if (disc->bytesPerSector != 512) return false;
	if (!SYS_IsDMAAddress(buffer, 32)) return false;
	if (!__MMCE[chan].attached) return false;

	return MMCE_WriteSectors(chan, sector, numSectors, buffer) == MMCE_RESULT_READY;
}

static bool __mmce_clearStatus(DISC_INTERFACE *disc)
{
	return true;
}

static bool __mmce_shutdown(DISC_INTERFACE *disc)
{
	s32 chan = (disc->ioType & 0xFF) - '0';

	if (disc->ioType < DEVICE_TYPE_GAMECUBE_MMCE(0)) return false;
	if (disc->ioType > DEVICE_TYPE_GAMECUBE_MMCE(2)) return false;
	if (!__MMCE[chan].attached) return false;

	MMCE_SetAccessMode(chan, MMCE_MODE_READONLY);

	if (chan == EXI_CHANNEL_2) {
		__MaskIrq(IM_PI_DEBUG);
		IRQ_Free(IRQ_PI_DEBUG);
	}

	__MMCE[chan].attached = false;
	EXI_Detach(chan);
	return true;
}

DISC_INTERFACE __io_mmcea = {
	DEVICE_TYPE_GAMECUBE_MMCE(0),
	FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE | FEATURE_GAMECUBE_SLOTA,
	__mmce_startup,
	__mmce_isInserted,
	__mmce_readSectors,
	__mmce_writeSectors,
	__mmce_clearStatus,
	__mmce_shutdown,
	0x100000000,
	512
};

DISC_INTERFACE __io_mmceb = {
	DEVICE_TYPE_GAMECUBE_MMCE(1),
	FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE | FEATURE_GAMECUBE_SLOTB,
	__mmce_startup,
	__mmce_isInserted,
	__mmce_readSectors,
	__mmce_writeSectors,
	__mmce_clearStatus,
	__mmce_shutdown,
	0x100000000,
	512
};

DISC_INTERFACE __io_mmce2 = {
	DEVICE_TYPE_GAMECUBE_MMCE(2),
	FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE | FEATURE_GAMECUBE_PORT2,
	__mmce_startup,
	__mmce_isInserted,
	__mmce_readSectors,
	__mmce_writeSectors,
	__mmce_clearStatus,
	__mmce_shutdown,
	0x100000000,
	512
};
