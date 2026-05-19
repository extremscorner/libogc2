/*-------------------------------------------------------------

n64.c -- N64 controller subsystem

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

#include <gctypes.h>
#include <ogc/irq.h>
#include <ogc/lwp.h>
#include <ogc/n64.h>
#include <ogc/si.h>
#include <ogc/system.h>

typedef struct {
	s32 result;
	u8 out[35];
	u8 in[33];
	u32 outLen;
	u32 inLen;
	u8 *status;
	void *ptr;
	N64Callback proc;
	N64Callback callback;
	lwpq_t syncQueue;
} N64ControlBlock;

static N64ControlBlock __N64[SI_MAX_CHAN];
static bool Reset;

static s32 OnReset(s32 final)
{
	Reset = true;
	return TRUE;
}

static sys_resetinfo ResetInfo = {
	.func = OnReset,
	.prio = 127
};

static void __attribute__((constructor)) __N64_Init(void)
{
	for (s32 chan = SI_CHAN0; chan < SI_MAX_CHAN; chan++)
		LWP_InitQueue(&__N64[chan].syncQueue);

	SYS_RegisterResetFunc(&ResetInfo);
}

static void __N64_SyncCallback(s32 chan, s32 result)
{
	LWP_ThreadBroadcast(__N64[chan].syncQueue);
}

static s32 N64_Sync(s32 chan)
{
	s32 result;
	u32 level = IRQ_Disable();

	while (__N64[chan].callback)
		if (LWP_ThreadSleep(__N64[chan].syncQueue)) break;

	result = __N64[chan].result;

	IRQ_Restore(level);
	return result;
}

static void __N64_Handler(s32 chan, u32 type)
{
	N64ControlBlock *cb = &__N64[chan];
	N64Callback callback, proc;

	if (Reset)
		return;
	else if (type & SI_ERROR_NO_RESPONSE)
		cb->result = N64_ERR_NO_CONTROLLER;
	else if (type & (SI_ERROR_COLLISION | SI_ERROR_OVER_RUN | SI_ERROR_UNDER_RUN))
		cb->result = N64_ERR_TRANSFER;
	else
		cb->result = N64_ERR_READY;

	if (cb->proc) {
		proc = cb->proc;
		cb->proc = NULL;
		proc(chan, cb->result);
	}

	if (cb->callback) {
		callback = cb->callback;
		cb->callback = NULL;
		callback(chan, cb->result);
	}
}

static void TypeCallback(s32 chan, u32 type)
{
	N64ControlBlock *cb = &__N64[chan];
	N64Callback callback, proc;

	if (Reset)
		return;
	else if (SI_DecodeType(type) != SI_N64_CONTROLLER)
		cb->result = N64_ERR_NO_CONTROLLER;
	else if (!SI_Transfer(chan, cb->out, cb->outLen, cb->in, cb->inLen, __N64_Handler, 0))
		cb->result = N64_ERR_BUSY;
	else
		return;

	if (cb->proc) {
		proc = cb->proc;
		cb->proc = NULL;
		proc(chan, cb->result);
	}

	if (cb->callback) {
		callback = cb->callback;
		cb->callback = NULL;
		callback(chan, cb->result);
	}
}

static s32 N64_Transfer(s32 chan, u32 outLen, u32 inLen, N64Callback proc)
{
	u32 level = IRQ_Disable();

	__N64[chan].outLen = outLen;
	__N64[chan].inLen = inLen;
	__N64[chan].proc = proc;
	SI_GetTypeAsync(chan, TypeCallback);

	IRQ_Restore(level);
	return N64_ERR_READY;
}

static void DefaultCallback(s32 chan, s32 result)
{
}

static void ShortCommandProc(s32 chan, s32 result)
{
	N64ControlBlock *cb = &__N64[chan];

	if (result == N64_ERR_READY) {
		if (cb->in[0] != 0x05 || cb->in[1] != 0x00) {
			cb->result = N64_ERR_NO_CONTROLLER;
			return;
		}

		*cb->status = cb->in[2];
	}
}

s32 N64_GetStatusAsync(s32 chan, u8 *status, N64Callback callback)
{
	N64ControlBlock *cb = &__N64[chan];

	if (cb->callback)
		return N64_ERR_BUSY;

	cb->out[0] = 0x00;
	cb->status = status;
	cb->callback = callback ? : DefaultCallback;
	return N64_Transfer(chan, 1, 3, ShortCommandProc);
}

s32 N64_GetStatus(s32 chan, u8 *status)
{
	s32 result = N64_GetStatusAsync(chan, status, __N64_SyncCallback);
	if (result == N64_ERR_READY) return N64_Sync(chan);
	return result;
}

s32 N64_ResetAsync(s32 chan, u8 *status, N64Callback callback)
{
	N64ControlBlock *cb = &__N64[chan];

	if (cb->callback)
		return N64_ERR_BUSY;

	cb->out[0] = 0xFF;
	cb->status = status;
	cb->callback = callback ? : DefaultCallback;
	return N64_Transfer(chan, 1, 3, ShortCommandProc);
}

s32 N64_Reset(s32 chan, u8 *status)
{
	s32 result = N64_ResetAsync(chan, status, __N64_SyncCallback);
	if (result == N64_ERR_READY) return N64_Sync(chan);
	return result;
}

static void ReadProc(s32 chan, s32 result)
{
	N64ControlBlock *cb = &__N64[chan];
	N64Status *status = (N64Status *)cb->ptr;

	if (result == N64_ERR_READY) {
		status->button = cb->in[1] << 8 | cb->in[0];
		status->stickX = cb->in[2];
		status->stickY = cb->in[3];
	}

	status->err = result;
}

s32 N64_ReadAsync(s32 chan, N64Status *status, N64Callback callback)
{
	N64ControlBlock *cb = &__N64[chan];

	if (cb->callback)
		return N64_ERR_BUSY;

	cb->out[0] = 0x01;
	cb->ptr = status;
	cb->callback = callback ? : DefaultCallback;
	return N64_Transfer(chan, 1, 4, ReadProc);
}

s32 N64_Read(s32 chan, N64Status *status)
{
	s32 result = N64_ReadAsync(chan, status, __N64_SyncCallback);
	if (result == N64_ERR_READY) return N64_Sync(chan);
	return result;
}
