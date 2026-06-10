/*-------------------------------------------------------------

si_steering.c -- Steering wheel support

Copyright (C) 2017 - 2026 Extrems' Corner.org

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
#include <ogc/machine/processor.h>
#include <ogc/si.h>
#include <ogc/si_steering.h>
#include <ogc/system.h>
#include <ogc/video.h>

typedef struct {
	s32 result;
	u8 out[3];
	u8 in[8];
	u32 outLen;
	u32 inLen;
	SISteeringCallback proc;
	SISteeringCallback callback;
	lwpq_t syncQueue;
} SISteeringControlBlock;

static SISteeringControlBlock __Steering[SI_MAX_CHAN];
static SISteeringSamplingCallback SamplingCallback;
static u32 EnabledBits;
static bool Reset;

static s32 OnReset(s32 final);

static sys_resetinfo ResetInfo = {
	.func = OnReset,
	.prio = 127
};

void SI_InitSteering(void)
{
	static bool initialized;

	if (initialized) return;
	initialized = true;

	for (s32 chan = SI_CHAN0; chan < SI_MAX_CHAN; chan++)
		LWP_InitQueue(&__Steering[chan].syncQueue);

	SI_RefreshSamplingRate();
	SYS_RegisterResetFunc(&ResetInfo);
}

static void __SI_SteeringSyncCallback(s32 chan, s32 result)
{
	LWP_ThreadBroadcast(__Steering[chan].syncQueue);
}

static s32 SI_SteeringSync(s32 chan)
{
	s32 result;
	u32 level = IRQ_Disable();

	while (__Steering[chan].callback)
		if (LWP_ThreadSleep(__Steering[chan].syncQueue)) break;

	result = __Steering[chan].result;

	IRQ_Restore(level);
	return result;
}

static void __SI_SteeringHandler(s32 chan, u32 type)
{
	SISteeringControlBlock *cb = &__Steering[chan];
	SISteeringCallback callback, proc;

	if (Reset)
		return;
	else if (type & SI_ERROR_NO_RESPONSE)
		cb->result = SI_STEERING_ERR_NO_CONTROLLER;
	else if (type & (SI_ERROR_COLLISION | SI_ERROR_OVER_RUN | SI_ERROR_UNDER_RUN))
		cb->result = SI_STEERING_ERR_TRANSFER;
	else
		cb->result = SI_STEERING_ERR_READY;

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
	SISteeringControlBlock *cb = &__Steering[chan];
	SISteeringCallback callback, proc;

	if (Reset)
		return;
	else if (SI_DecodeType(type) != SI_GC_STEERING)
		cb->result = SI_STEERING_ERR_NO_CONTROLLER;
	else if (!SI_Transfer(chan, cb->out, cb->outLen, cb->in, cb->inLen, __SI_SteeringHandler, 0))
		cb->result = SI_STEERING_ERR_BUSY;
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

static s32 SI_SteeringTransfer(s32 chan, u32 outLen, u32 inLen, SISteeringCallback proc)
{
	u32 level = IRQ_Disable();

	__Steering[chan].outLen = outLen;
	__Steering[chan].inLen = inLen;
	__Steering[chan].proc = proc;
	SI_GetTypeAsync(chan, TypeCallback);

	IRQ_Restore(level);
	return SI_STEERING_ERR_READY;
}

static void SI_SteeringEnable(s32 chan)
{
	u32 mask = SI_CHAN_BIT(chan);
	u8 buf[8];

	if (!(EnabledBits & mask)) {
		EnabledBits |= mask;
		SI_GetResponse(chan, buf);
		SI_SetCommand(chan, 0x300680);
		SI_EnablePolling(EnabledBits);
	}
}

static void SI_SteeringDisable(s32 chan)
{
	u32 mask = SI_CHAN_BIT(chan);
	SI_DisablePolling(mask);
	EnabledBits &= ~mask;
}

static void DefaultCallback(s32 chan, s32 result)
{
}

static void ResetProc(s32 chan, s32 result)
{
	if (result == SI_STEERING_ERR_READY && !Reset)
		SI_SteeringEnable(chan);
}

s32 SI_ResetSteeringAsync(s32 chan, SISteeringCallback callback)
{
	SISteeringControlBlock *cb = &__Steering[chan];

	if (cb->callback)
		return SI_STEERING_ERR_BUSY;

	cb->out[0] = 0xFF;
	cb->callback = callback ? : DefaultCallback;
	return SI_SteeringTransfer(chan, 1, 3, ResetProc);
}

s32 SI_ResetSteering(s32 chan)
{
	s32 result = SI_ResetSteeringAsync(chan, __SI_SteeringSyncCallback);
	if (result == SI_STEERING_ERR_READY) return SI_SteeringSync(chan);
	return result;
}

static s32 OnReset(s32 final)
{
	static u32 retraceCount;

	if (SamplingCallback)
		SI_SetSteeringSamplingCallback(NULL);

	if (!Reset) {
		Reset = true;

		for (s32 chan = SI_CHAN0; chan < SI_MAX_CHAN; chan++)
			SI_ControlSteering(chan, SI_STEERING_CONTROL_DRIVE, 0);

		retraceCount = VIDEO_GetRetraceCount();
	}

	if ((s32)(VIDEO_GetRetraceCount() - retraceCount) > 2) {
		while (EnabledBits) {
			s32 chan = cntlzw(EnabledBits);
			SI_SteeringDisable(chan);
		}
	}

	return !EnabledBits;
}

s32 SI_ReadSteering(s32 chan, SISteeringStatus *status)
{
	s32 result;
	u32 level = IRQ_Disable();
	u8 buf[8];

	if (SI_IsChanBusy(chan)) {
		result = SI_STEERING_ERR_BUSY;
	} else if (!(EnabledBits & SI_CHAN_BIT(chan))) {
		result = SI_STEERING_ERR_NO_CONTROLLER;
	} else if (SI_GetStatus(chan) & SI_ERROR_NO_RESPONSE) {
		SI_GetResponse(chan, buf);
		SI_SteeringDisable(chan);
		result = SI_STEERING_ERR_NO_CONTROLLER;
	} else if (!SI_GetResponse(chan, buf) || (((u32 *)buf)[0] & 0x80000000)) {
		result = SI_STEERING_ERR_TRANSFER;
	} else {
		result = SI_STEERING_ERR_READY;

		if (status) {
			status->button   = buf[0] << 8 | buf[1];
			status->status   = buf[2];
			status->steering = buf[3] - 128;
			status->gas      = buf[4];
			status->brake    = buf[5];
			status->left     = buf[6];
			status->right    = buf[7];
		}
	}

	if (status) status->err = result;
	__Steering[chan].result = result;

	IRQ_Restore(level);
	return result;
}

static void SamplingHandler(u32 irq, void *ctx)
{
	if (SamplingCallback)
		SamplingCallback();
}

SISteeringSamplingCallback SI_SetSteeringSamplingCallback(SISteeringSamplingCallback callback)
{
	SISteeringSamplingCallback old;

	old = SamplingCallback;
	SamplingCallback = callback;

	if (callback)
		SI_RegisterPollingHandler(SamplingHandler);
	else
		SI_UnregisterPollingHandler(SamplingHandler);

	return old;
}

void SI_ControlSteering(s32 chan, u32 control, s32 steering)
{
	if (steering < -128) steering = -128;
	if (steering > +128) steering = +128;

	u32 level = IRQ_Disable();

	if (EnabledBits & SI_CHAN_BIT(chan)) {
		u32 cmd = 0x300000 | (control & 0x600) | ((steering + 128) & 0x1FF);
		SI_SetCommand(chan, cmd);
		SI_TransferCommands();
	}

	IRQ_Restore(level);
}
