#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <irq.h>
#include <video.h>
#include <system.h>
#include "asm.h"
#include "processor.h"
#include "si.h"
#include "si_steering.h"
#include "pad.h"
#include "n64.h"

//#define _PAD_DEBUG

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))

typedef struct _keyinput {
	s8 stickX;
	s8 stickY;
	s8 substickX;
	s8 substickY;
	u8 triggerL;
	u8 triggerR;
	u8 analogA;
	u8 analogB;
	u32 up;
	u32 down;
	u32 state;
	s32 chan;
} keyinput;

typedef void (*SPECCallback)(u32 chan,u32 *data,PADStatus *status);

static void SPEC0_MakeStatus(u32 chan,u32 *data,PADStatus *status);
static void SPEC1_MakeStatus(u32 chan,u32 *data,PADStatus *status);
static void SPEC2_MakeStatus(u32 chan,u32 *data,PADStatus *status);

static SPECCallback __pad_makestatus = SPEC2_MakeStatus;
static sampling_callback __pad_samplingcallback = NULL;
static u32 __pad_initialized = 0;
static u32 __pad_enabledbits = 0;
static u32 __pad_resettingbits = 0;
static u32 __pad_recalibratebits = 0;
static u32 __pad_waitingbits = 0;
static u32 __pad_pendingbits = 0;
static u32 __pad_checkingbits = 0;
static u32 __pad_barrelbits = 0;
static u32 __pad_resettingchan = 32;
static u32 __pad_spec = 5;

static u32 __pad_analogmode = 0x00000300;
static u32 __pad_cmdreadorigin = 0x41000000;
static u32 __pad_cmdcalibrate = 0x42000000;
static u32 __pad_xpatchbits = 0xf0000000;

static u32 __pad_recalibrated$207 = 0;

static u32 __pad_type[PAD_CHANMAX];
static PADStatus __pad_origin[PAD_CHANMAX];
static u32 __pad_cmdprobedevice[PAD_CHANMAX];

static keyinput __pad_keys[PAD_CHANMAX];
static u8 __pad_clampregion[8] = {30, 180, 15, 72, 40, 15, 59, 31};

extern u32 __PADFixBits;

static void __pad_enable(u32 chan);
static void __pad_disable(u32 chan);
static void __pad_doreset(void);
static s32 __pad_onreset(s32 final);

static sys_resetinfo pad_resetinfo = {
	{},
	__pad_onreset,
	127
};

static s32 __pad_onreset(s32 final)
{
	u32 ret;

	if(__pad_samplingcallback!=NULL) PAD_SetSamplingCallback(NULL);

	if(final==FALSE) {
		ret = PAD_Sync();
		if(__pad_recalibrated$207==0 && ret) {
			__pad_recalibrated$207 = PAD_Recalibrate(0xf0000000);
			return 0;
		}
		return ret;
	}
	__pad_recalibrated$207 = 0;
	return 1;
}

static void SPEC0_MakeStatus(u32 chan,u32 *data,PADStatus *status)
{
	status->button = 0;

	if(data[0]&0x00080000) status->button |= PAD_BUTTON_A;
	if(data[0]&0x00200000) status->button |= PAD_BUTTON_B;
	if(data[0]&0x01000000) status->button |= PAD_BUTTON_X;
	if(data[0]&0x00010000) status->button |= PAD_BUTTON_Y;
	if(data[0]&0x00100000) status->button |= PAD_BUTTON_MENU;

	status->stickX = (s8)(data[1]>>16);
	status->stickY = (s8)(data[1]>>24);
	status->substickX = (s8)data[1];
	status->substickY = (s8)(data[1]>>8);
	status->triggerL = (u8)_SHIFTR(data[0],8,8);
	status->triggerR = (u8)(data[0]&0xff);
	status->analogA = 0;
	status->analogB = 0;

	if(status->triggerL>=0xaa) status->button |= PAD_TRIGGER_L;
	if(status->triggerR>=0xaa) status->button |= PAD_TRIGGER_R;

	status->stickX -= 128;
	status->stickY -= 128;
	status->substickX -= 128;
	status->substickY -= 128;
}

static void SPEC1_MakeStatus(u32 chan,u32 *data,PADStatus *status)
{
	status->button = 0;

	if(data[0]&0x00800000) status->button |= PAD_BUTTON_A;
	if(data[0]&0x01000000) status->button |= PAD_BUTTON_B;
	if(data[0]&0x00200000) status->button |= PAD_BUTTON_X;
	if(data[0]&0x00100000) status->button |= PAD_BUTTON_Y;
	if(data[0]&0x02000000) status->button |= PAD_BUTTON_MENU;

	status->stickX = (s8)(data[1]>>16);
	status->stickY = (s8)(data[1]>>24);
	status->substickX = (s8)data[1];
	status->substickY = (s8)(data[1]>>8);
	status->triggerL = (u8)_SHIFTR(data[0],8,8);
	status->triggerR = (u8)data[0]&0xff;
	status->analogA = 0;
	status->analogB = 0;

	if(status->triggerL>=0xaa) status->button |= PAD_TRIGGER_L;
	if(status->triggerR>=0xaa) status->button |= PAD_TRIGGER_R;

	status->stickX -= 128;
	status->stickY -= 128;
	status->substickX -= 128;
	status->substickY -= 128;
}

static s8 __pad_clampS8(s8 var,s8 org)
{
	s32 siorg;

#ifdef _PAD_DEBUG
	u32 tmp = var;
	printf("__pad_clampS8(%d,%d)\n",var,org);
#endif
	siorg = (s32)org;
	if(siorg>=0) {
		siorg -= INT8_MAX;
		if((s32)var<siorg) var = siorg;
	} else {
		siorg += INT8_MAX;
		if(siorg<(s32)var) var = siorg;
	}
#ifdef _PAD_DEBUG
	printf("__pad_clampS8(%d,%d,%d,%d)\n",tmp,var,org,(var-org));
#endif
	return (var-org);
}

static u8 __pad_clampU8(u8 var,u8 org)
{
	if(var<org) var = org;
	return (var-org);
}

static void SPEC2_MakeStatus(u32 chan,u32 *data,PADStatus *status)
{
	u32 mode,type;

	status->button = _SHIFTR(data[0],16,14);

	status->stickX = (s8)(data[0]>>8);
	status->stickY = (s8)data[0];
#ifdef _PAD_DEBUG
	printf("SPEC2_MakeStatus(%d,%p,%p)",chan,data,status);
#endif
	mode = __pad_analogmode&0x0700;
	if(mode==0x100) {
		status->substickX = (s8)((data[1]>>24)&0xf0);
		status->substickY = (s8)((data[1]>>20)&0xf0);
		status->triggerL = (u8)((data[1]>>16)&0xff);
		status->triggerR = (u8)((data[1]>>8)&0xff);
		status->analogA = (u8)(data[1]&0xf0);
		status->analogB = (u8)((data[1]<<4)&0xf0);
	} else if(mode==0x200) {
		status->substickX = (s8)((data[1]>>24)&0xf0);
		status->substickY = (s8)((data[1]>>20)&0xf0);
		status->triggerL = (u8)((data[1]>>16)&0xf0);
		status->triggerR = (u8)((data[1]>>12)&0xf0);
		status->analogA = (u8)((data[1]>>8)&0xff);
		status->analogB = (s8)data[1]&0xff;
	} else if(mode==0x300) {
		status->substickX = (s8)((data[1]>>24)&0xff);
		status->substickY = (s8)((data[1]>>16)&0xff);
		status->triggerL = (u8)((data[1]>>8)&0xff);
		status->triggerR = (u8)data[1]&0xff;
		status->analogA = 0;
		status->analogB = 0;
	} else if(mode==0x400) {
		status->substickX = (s8)((data[1]>>24)&0xff);
		status->substickY = (s8)((data[1]>>16)&0xff);
		status->triggerL = 0;
		status->triggerR = 0;
		status->analogA = (u8)((data[1]>>8)&0xff);
		status->analogB = (u8)data[1]&0xff;
	} else if(!mode || mode==0x500 || mode==0x600 || mode==0x700) {
		status->substickX = (s8)((data[1]>>24)&0xff);
		status->substickY = (s8)((data[1]>>16)&0xff);
		status->triggerL = (u8)((data[1]>>8)&0xf0);
		status->triggerR = (u8)((data[1]>>4)&0xf0);
		status->analogA = (u8)(data[1]&0xf0);
		status->analogB = (u8)((data[1]<<4)&0xf0);
	}

	status->stickX -= 128;
	status->stickY -= 128;
	status->substickX -= 128;
	status->substickY -= 128;

	type = __pad_type[chan]&~0xffff;
	if(type==SI_GC_CONTROLLER && !(status->button&0x80)) {
		__pad_barrelbits |= PAD_CHAN_BIT(chan);
		status->stickX = 0;
		status->stickY = 0;
		status->substickX = 0;
		status->substickY = 0;
	} else {
		__pad_barrelbits &= ~PAD_CHAN_BIT(chan);
		status->stickX = __pad_clampS8(status->stickX,__pad_origin[chan].stickX);
		status->stickY = __pad_clampS8(status->stickY,__pad_origin[chan].stickY);
		status->substickX = __pad_clampS8(status->substickX,__pad_origin[chan].substickX);
		status->substickY = __pad_clampS8(status->substickY,__pad_origin[chan].substickY);
		status->triggerL = __pad_clampU8(status->triggerL,__pad_origin[chan].triggerL);
		status->triggerR = __pad_clampU8(status->triggerR,__pad_origin[chan].triggerR);
	}
}

static void __pad_clampstick(s8 *px,s8 *py,s8 max,s8 xy,s8 min)
{
	s32 x,y,signX,signY,d;

	x = *px;
	y = *py;
	if(x>=0) signX = 1;
	else { signX = -1; x = -(x); }

	if(y>=0) signY = 1;
	else { signY = -1; y = -(y); }

	if(x<=min) x = 0;
	else x -= min;

	if(y<=min) y = 0;
	else y -= min;

	if(x!=0 || y!=0) {
		s32 xx,yy,maxy;

		xx = (x * xy);
		yy= (y * xy);
		maxy = (max * xy);
		if(yy<=xx) {
			d = ((x * xy) + (y * (max - xy)));
			if(maxy<d) {
				x = ((x * maxy)/d);
				y = ((y * maxy)/d);
			}
		} else {
			d = ((y * xy) + (x * (max - xy)));
			if(maxy<d) {
				x = ((x * maxy)/d);
				y = ((y * maxy)/d);
			}
		}
		*px = (s8)(x * signX);
		*py = (s8)(y * signY);
	} else
		*px = *py = 0;
}

static void __pad_clamptrigger(u8 *trigger)
{
	u8 min, max;

	min = __pad_clampregion[0];
	max = __pad_clampregion[1];
	if(min>*trigger) *trigger = 0;
	else if(max<*trigger) *trigger = (max - min);
	else *trigger -= min;
}

static void __pad_updateorigin(s32 chan)
{
	u32 mode,mask,type;

	mask = PAD_CHAN_BIT(chan);
	mode = __pad_analogmode&0x0700;
	if(mode==0x100) {
		__pad_origin[chan].substickX &= ~0x0f;
		__pad_origin[chan].substickY &= ~0x0f;
		__pad_origin[chan].analogA &= ~0x0f;
		__pad_origin[chan].analogB &= ~0x0f;
	} else if(mode==0x200) {
		__pad_origin[chan].substickX &= ~0x0f;
		__pad_origin[chan].substickY &= ~0x0f;
		__pad_origin[chan].triggerL &= ~0x0f;
		__pad_origin[chan].triggerR &= ~0x0f;
	} else if(!mode || mode==0x500 || mode==0x600 || mode==0x700) {
		__pad_origin[chan].triggerL &= ~0x0f;
		__pad_origin[chan].triggerR &= ~0x0f;
		__pad_origin[chan].analogA &= ~0x0f;
		__pad_origin[chan].analogB &= ~0x0f;
	}

	__pad_origin[chan].stickX -= 128;
	__pad_origin[chan].stickY -= 128;
	__pad_origin[chan].substickX -= 128;
	__pad_origin[chan].substickY -= 128;

	if(__pad_xpatchbits&mask && __pad_origin[chan].stickX>64) {
		type = SI_GetType(chan)&~0xffff;
		if(type==SI_GC_CONTROLLER) __pad_origin[chan].stickX = 0;
	}
}

static void __pad_probecallback(s32 chan,u32 type)
{
	if(!(type&0x0f)) {
		__pad_enable(__pad_resettingchan);
		__pad_waitingbits |= PAD_CHAN_BIT(__pad_resettingchan);
	}
	__pad_doreset();
}

static void __pad_origincallback(s32 chan,u32 type)
{
#ifdef _PAD_DEBUG
	printf("__pad_origincallback(%d,%08x)\n",chan,type);
#endif
	if(!(type&0x0f)) {
		__pad_updateorigin(__pad_resettingchan);
		__pad_enable(__pad_resettingchan);
	}
	__pad_doreset();
}

static void __pad_originupdatecallback(s32 chan,u32 type)
{
	u32 en_bits = __pad_enabledbits&PAD_CHAN_BIT(chan);

	if(en_bits) {
		if(!(type&0x0f)) __pad_updateorigin(chan);
		if(type&SI_ERROR_NO_RESPONSE) __pad_disable(chan);
	}
}

static void __pad_typeandstatuscallback(s32 chan,u32 type)
{
	u32 recal_bits,mask,ret = 0;
#ifdef _PAD_DEBUG
	printf("__pad_typeandstatuscallback(%d,%08x)\n",chan,type);
#endif
	mask = PAD_CHAN_BIT(__pad_resettingchan);
	recal_bits = __pad_recalibratebits&mask;
	__pad_recalibratebits &= ~mask;

	if(type&0x0f) {
		__pad_doreset();
		return;
	}

	__pad_type[__pad_resettingchan] = type&~0xff;
	if((type&SI_TYPE_MASK)!=SI_TYPE_GC
		|| !(type&SI_GC_STANDARD)) {
		__pad_doreset();
		return;
	}

	if(__pad_spec<2) {
		__pad_enable(__pad_resettingchan);
		__pad_doreset();
		return;
	}

	if(!(type&SI_GC_WIRELESS) || type&SI_WIRELESS_IR) {
		if(recal_bits) ret = SI_Transfer(__pad_resettingchan,&__pad_cmdcalibrate,3,&__pad_origin[__pad_resettingchan],10,__pad_origincallback,0);
		else ret = SI_Transfer(__pad_resettingchan,&__pad_cmdreadorigin,1,&__pad_origin[__pad_resettingchan],10,__pad_origincallback,0);
	} else if(type&SI_WIRELESS_FIX_ID && (type&SI_WIRELESS_LITE_MASK)==SI_WIRELESS_CONT) {
		if(type&SI_WIRELESS_RECEIVED) ret = SI_Transfer(__pad_resettingchan,&__pad_cmdreadorigin,1,&__pad_origin[__pad_resettingchan],10,__pad_origincallback,0);
		else ret = SI_Transfer(__pad_resettingchan,&__pad_cmdprobedevice[__pad_resettingchan],3,&__pad_origin[__pad_resettingchan],8,__pad_probecallback,0);
	}
	if(!ret) {
		__pad_pendingbits |= mask;
		__pad_doreset();
	}
}

static void __pad_receivecheckcallback(s32 chan,u32 type)
{
	u32 mask,tmp;
#ifdef _PAD_DEBUG
	printf("__pad_receivecheckcallback(%d,%08x)\n",chan,type);
#endif
	mask = PAD_CHAN_BIT(chan);
	if(__pad_enabledbits&mask) {
		tmp = type&0xff;
		type &= ~0xff;
		__pad_waitingbits &= ~mask;
		__pad_checkingbits &= ~mask;
		if(!(tmp&0x0f)
			&& (type&SI_GC_WIRELESS) && (type&SI_WIRELESS_RECEIVED) && (type&SI_WIRELESS_FIX_ID)
			&& !(type&SI_WIRELESS_IR) && (type&SI_WIRELESS_LITE_MASK)==SI_WIRELESS_CONT) SI_Transfer(chan,&__pad_cmdreadorigin,1,&__pad_origin[chan],10,__pad_originupdatecallback,0);
		else __pad_disable(chan);
	}
}

static void __pad_enable(u32 chan)
{
	u32 buf[2];
#ifdef _PAD_DEBUG
	printf("__pad_enable(%d)\n",chan);
#endif
	__pad_enabledbits |= PAD_CHAN_BIT(chan);
	SI_GetResponse(chan,(void*)buf);
	SI_SetCommand(chan,(__pad_analogmode|0x00400000));
	SI_EnablePolling(__pad_enabledbits);
}

static void __pad_disable(u32 chan)
{
	u32 level,mask;
#ifdef _PAD_DEBUG
	printf("__pad_disable(%d)\n",chan);
#endif
	_CPU_ISR_Disable(level);
	mask = PAD_CHAN_BIT(chan);
	SI_DisablePolling(mask);
	__pad_enabledbits &= ~mask;
	__pad_waitingbits &= ~mask;
	__pad_pendingbits &= ~mask;
	__pad_checkingbits &= ~mask;
	__pad_barrelbits &= ~mask;
	SYS_SetWirelessID(chan,0);
	_CPU_ISR_Restore(level);
}

static void __pad_doreset(void)
{
	__pad_resettingchan = cntlzw(__pad_resettingbits);
	if(__pad_resettingchan==32) return;
#ifdef _PAD_DEBUG
	printf("__pad_doreset(%d)\n",__pad_resettingchan);
#endif
	__pad_resettingbits &= ~PAD_CHAN_BIT(__pad_resettingchan);

	memset(&__pad_origin[__pad_resettingchan],0,sizeof(PADStatus));
	SI_GetTypeAsync(__pad_resettingchan,__pad_typeandstatuscallback);
}

static void __pad_samplinghandler(u32 irq,void *ctx)
{
	if(__pad_samplingcallback)
		__pad_samplingcallback();
}

u32 __PADDisableRecalibration(s32 disable)
{
	u32 level,ret;
	u8 *flags = (u8*)0x800030e3;

	_CPU_ISR_Disable(level);

	if(*flags&0x40) ret = 1;
	else ret = 0;

	*flags &= ~0x40;
	if(disable) *flags |= 0x40;

	_CPU_ISR_Restore(level);

	return ret;
}

u32 __PADDisableRumble(s32 disable)
{
	u32 level,ret;
	u8 *flags = (u8*)0x800030e3;

	_CPU_ISR_Disable(level);

	if(*flags&0x20) ret = 1;
	else ret = 0;

	*flags &= ~0x20;
	if(disable) *flags |= 0x20;

	_CPU_ISR_Restore(level);

	return ret;
}

void __PADDisableXPatch(void)
{
	__pad_xpatchbits = 0;
}

u32 PAD_Init(void)
{
	u32 chan;
	u16 *fix_mode = (u16*)0x800030e0;
	u8 *spec = (u8*)0x800030e9;
#ifdef _PAD_DEBUG
	printf("PAD_Init()\n");
#endif
	if(__pad_initialized) return 1;
	if(*spec) PAD_SetSpec(*spec);
	__pad_initialized = 1;

	memset(__pad_keys,0,sizeof(keyinput)*PAD_CHANMAX);

	chan = 0;
	while(chan<4) {
		__pad_cmdprobedevice[chan] = 0x4d000000|(chan<<22)|_SHIFTL(*fix_mode,8,14);
		__pad_keys[chan].chan = -1;
		chan++;
	}

	SI_RefreshSamplingRate();
	SYS_RegisterResetFunc(&pad_resetinfo);

	return PAD_Recalibrate(0xf0000000);
}

u32 PAD_Read(PADStatus *status)
{
	u32 chan,mask,ret;
	u32 level,sistatus,type;
	u32 buf[2];
#ifdef _PAD_DEBUG
	printf("PAD_Read(%p)\n",status);
#endif
	_CPU_ISR_Disable(level);
	chan = 0;
	ret = 0;
	while(chan<4) {
		mask = PAD_CHAN_BIT(chan);
#ifdef _PAD_DEBUG
		printf("PAD_Read(%d,%d,%08x,%08x,%08x)\n",chan,__pad_resettingchan,(__pad_pendingbits&mask),(__pad_resettingbits&mask),(__pad_enabledbits&mask));
#endif
		if(__pad_pendingbits&mask) {
			PAD_Reset(0);
			memset(&status[chan],0,sizeof(PADStatus));
			status[chan].err = PAD_ERR_NOT_READY;
		} else if(__pad_resettingbits&mask || __pad_resettingchan==chan) {
			memset(&status[chan],0,sizeof(PADStatus));
			status[chan].err = PAD_ERR_NOT_READY;
		} else if(!(__pad_enabledbits&mask)) {
			memset(&status[chan],0,sizeof(PADStatus));
			status[chan].err = PAD_ERR_NO_CONTROLLER;
		} else {
			if(SI_IsChanBusy(chan)) {
				memset(&status[chan],0,sizeof(PADStatus));
				status[chan].err = PAD_ERR_TRANSFER;
			} else {
				sistatus = SI_GetStatus(chan);
#ifdef _PAD_DEBUG
				printf("PAD_Read(%08x)\n",sistatus);
#endif
				if(sistatus&SI_ERROR_NO_RESPONSE) {
#ifdef _PAD_DEBUG
					printf("PAD_Read(%08x)\n",sistatus);
#endif
					SI_GetResponse(chan,(void*)buf);
					if(__pad_waitingbits&mask) {
						memset(&status[chan],0,sizeof(PADStatus));
						status[chan].err = PAD_ERR_NONE;
						if(!(__pad_checkingbits&mask)) {
							__pad_checkingbits |= mask;
							SI_GetTypeAsync(chan,__pad_receivecheckcallback);
						}
					} else {
						__pad_disable(chan);
						memset(&status[chan],0,sizeof(PADStatus));
						status[chan].err = PAD_ERR_NO_CONTROLLER;
					}
				} else {
					type = SI_GetType(chan);
#ifdef _PAD_DEBUG
					printf("PAD_Read(%08x)\n",type);
#endif
					if(!(type&SI_GC_NOMOTOR)) ret |= mask;
					if(!SI_GetResponse(chan,buf)
						|| buf[0]&0x80000000) {
#ifdef _PAD_DEBUG
						printf("PAD_Read(%08x %08x)\n",buf[0],buf[1]);
#endif
						memset(&status[chan],0,sizeof(PADStatus));
						status[chan].err = PAD_ERR_TRANSFER;
					} else {
#ifdef _PAD_DEBUG
						printf("PAD_Read(%08x %08x)\n",buf[0],buf[1]);
#endif
						__pad_makestatus(chan,buf,&status[chan]);
#ifdef _PAD_DEBUG
						printf("PAD_Read(%08x)\n",status[chan].button);
#endif
						if(status[chan].button&0x00002000) {
							memset(&status[chan],0,sizeof(PADStatus));
							status[chan].err = PAD_ERR_TRANSFER;
							SI_Transfer(chan,&__pad_cmdreadorigin,1,&__pad_origin[chan],10,__pad_originupdatecallback,0);
						} else {
							status[chan].err = PAD_ERR_NONE;
							status[chan].button &= ~0x80;
						}
					}
				}
			}
		}
		chan++;

	}
	_CPU_ISR_Restore(level);

	return ret;
}

void PAD_GetOrigin(PADStatus *origin)
{
	u32 chan,mask;
	u32 level;

	_CPU_ISR_Disable(level);
	chan = 0;
	while(chan<4) {
		mask = PAD_CHAN_BIT(chan);
		if(__pad_pendingbits&mask) {
			PAD_Reset(0);
			memset(&origin[chan],0,sizeof(PADStatus));
			origin[chan].err = PAD_ERR_NOT_READY;
		} else if(__pad_resettingbits&mask || __pad_resettingchan==chan) {
			memset(&origin[chan],0,sizeof(PADStatus));
			origin[chan].err = PAD_ERR_NOT_READY;
		} else if(!(__pad_enabledbits&mask)) {
			memset(&origin[chan],0,sizeof(PADStatus));
			origin[chan].err = PAD_ERR_NO_CONTROLLER;
		} else {
			memcpy(&origin[chan],&__pad_origin[chan],sizeof(PADStatus));
			origin[chan].err = PAD_ERR_NONE;
			origin[chan].button &= ~0x80;
		}
		chan++;
	}
	_CPU_ISR_Restore(level);
}

u32 PAD_Reset(u32 mask)
{
	u32 level;
	u32 pend_bits,en_bits;

	_CPU_ISR_Disable(level);
	pend_bits = (__pad_pendingbits|mask);
	__pad_pendingbits = 0;

	pend_bits &= ~(__pad_waitingbits|__pad_checkingbits);
	__pad_resettingbits |= pend_bits;

	en_bits = (__pad_resettingbits&__pad_enabledbits);
	__pad_enabledbits &= ~pend_bits;
	__pad_barrelbits &= ~pend_bits;

	if(__pad_spec==4) __pad_recalibratebits |= pend_bits;

	SI_DisablePolling(en_bits);
	if(__pad_resettingchan==32) __pad_doreset();
	_CPU_ISR_Restore(level);

	return 1;
}

u32 PAD_Recalibrate(u32 mask)
{
	u32 level;
	u32 pend_bits,en_bits;
	u8 *flags = (u8*)0x800030e3;

	_CPU_ISR_Disable(level);
	pend_bits = (__pad_pendingbits|mask);
	__pad_pendingbits = 0;

	pend_bits &= ~(__pad_waitingbits|__pad_checkingbits);
	__pad_resettingbits |= pend_bits;

	en_bits = (__pad_resettingbits&__pad_enabledbits);
	__pad_enabledbits &= ~pend_bits;
	__pad_barrelbits &= ~pend_bits;

	if(!(*flags&0x40)) __pad_recalibratebits |= pend_bits;

	SI_DisablePolling(en_bits);
	if(__pad_resettingchan==32) __pad_doreset();
	_CPU_ISR_Restore(level);

	return 1;
}

u32 PAD_Sync(void)
{
	u32 ret = 0;

	if(!__pad_resettingbits && __pad_resettingchan==32) {
		if(SI_Busy()==0) ret = 1;
	}
	return ret;
}

void PAD_SetAnalogMode(u32 mode)
{
	u32 level;
	u32 en_bits;

	_CPU_ISR_Disable(level);
	en_bits = __pad_enabledbits;
	__pad_analogmode = mode<<8;
	__pad_enabledbits &= ~en_bits;
	__pad_waitingbits &= ~en_bits;
	__pad_checkingbits &= ~en_bits;
	SI_DisablePolling(en_bits);
	_CPU_ISR_Restore(level);
}

void PAD_SetSpec(u32 spec)
{
	if(__pad_initialized) return;

	*(u8*)0x800030e9 = spec;
	if(spec==0) __pad_makestatus = SPEC0_MakeStatus;
	else if(spec==1) __pad_makestatus = SPEC1_MakeStatus;
	else if(spec<=5) __pad_makestatus = SPEC2_MakeStatus;

	__pad_spec = spec;
}

u32 PAD_GetSpec(void)
{
	return __pad_spec;
}

u32 PAD_GetType(s32 chan,u32 *type)
{
	u32 mask;

	*type = SI_GetType(chan);
	mask = PAD_CHAN_BIT(chan);

	if(__pad_resettingbits&mask || __pad_resettingchan==chan) {
		return 0;
	}
	return !!(__pad_enabledbits&mask);
}

u32 PAD_IsBarrel(s32 chan)
{
	if(chan<PAD_CHAN0 || chan>PAD_CHAN3) return 0;
	return !!(__pad_barrelbits&PAD_CHAN_BIT(chan));
}

void PAD_ControlAllMotors(const u32 *cmds)
{
	u32 level;
	u32 chan,cmd,ret;
	u32 mask,type;
	u8 *flags = (u8*)0x800030e3;

	_CPU_ISR_Disable(level);
	chan = 0;
	ret = 0;
	while(chan<4) {
		mask = PAD_CHAN_BIT(chan);
		if(__pad_enabledbits&mask) {
			type = SI_GetType(chan);
			if(!(type&SI_GC_NOMOTOR)) {
				cmd = cmds[chan];
				if((__pad_spec<2 && cmd==PAD_MOTOR_STOP_HARD) || *flags&0x20) cmd = PAD_MOTOR_STOP;

				cmd = 0x00400000|__pad_analogmode|(cmd&0x03);
				SI_SetCommand(chan,cmd);
				ret = 1;
			}
		}
		chan++;
	}
	if(ret) SI_TransferCommands();
	_CPU_ISR_Restore(level);
}

void PAD_ControlMotor(s32 chan,u32 cmd)
{
	u32 level;
	u32 mask,type;
	u8 *flags = (u8*)0x800030e3;

	_CPU_ISR_Disable(level);

	mask = PAD_CHAN_BIT(chan);
	if(__pad_enabledbits&mask) {
		type = SI_GetType(chan);
		if(!(type&SI_GC_NOMOTOR)) {
			if((__pad_spec<2 && cmd==PAD_MOTOR_STOP_HARD) || *flags&0x20) cmd = PAD_MOTOR_STOP;

			cmd = 0x00400000|__pad_analogmode|(cmd&0x03);
			SI_SetCommand(chan,cmd);
			SI_TransferCommands();
		}
	}
	_CPU_ISR_Restore(level);
}

void PAD_SetSamplingRate(u32 samplingrate)
{
	SI_SetSamplingRate(samplingrate);
}

sampling_callback PAD_SetSamplingCallback(sampling_callback cb)
{
	sampling_callback ret;

	ret = __pad_samplingcallback;
	__pad_samplingcallback = cb;
	if(cb) {
		SI_RegisterPollingHandler(__pad_samplinghandler);
	} else {
		SI_UnregisterPollingHandler(__pad_samplinghandler);
	}

	return ret;
}

void PAD_Clamp(PADStatus *status)
{
	s32 i;

	for(i=0;i<PAD_CHANMAX;i++) {
		if(status[i].err==PAD_ERR_NONE) {
			__pad_clampstick(&status[i].stickX,&status[i].stickY,__pad_clampregion[3],__pad_clampregion[4],__pad_clampregion[2]);
			__pad_clampstick(&status[i].substickX,&status[i].substickY,__pad_clampregion[6],__pad_clampregion[7],__pad_clampregion[5]);
			__pad_clamptrigger(&status[i].triggerL);
			__pad_clamptrigger(&status[i].triggerR);
		}
	}
}

u32 PAD_ScanPads(void)
{
	s32 i;
	u32 resetBits;
	u32 padBit,connected;
	u32 state,oldstate;
	SISteeringStatus steering;
	PADStatus padstatus[PAD_CHANMAX];
	N64Status n64status;

	resetBits = 0;
	connected = 0;

	PAD_Init();
	PAD_Read(padstatus);
	//PAD_Clamp(padstatus);
	for(i=0;i<PAD_CHANMAX;i++) {
		padBit = PAD_CHAN_BIT(i);

		switch(padstatus[i].err) {
		case PAD_ERR_NONE:
			state					= padstatus[i].button;
			__pad_keys[i].stickX	= padstatus[i].stickX;
			__pad_keys[i].stickY	= padstatus[i].stickY;
			__pad_keys[i].substickX	= padstatus[i].substickX;
			__pad_keys[i].substickY	= padstatus[i].substickY;
			__pad_keys[i].triggerL	= padstatus[i].triggerL;
			__pad_keys[i].triggerR	= padstatus[i].triggerR;
			__pad_keys[i].analogA	= padstatus[i].analogA;
			__pad_keys[i].analogB	= padstatus[i].analogB;

			if(!PAD_IsBarrel(i)) {
				if(padstatus[i].stickX<(INT8_MIN/2))	state |= PADEX_STICK_LEFT;
				if(padstatus[i].stickX>(INT8_MAX/2))	state |= PADEX_STICK_RIGHT;
				if(padstatus[i].stickY<(INT8_MIN/2))	state |= PADEX_STICK_DOWN;
				if(padstatus[i].stickY>(INT8_MAX/2))	state |= PADEX_STICK_UP;
				if(padstatus[i].substickX<(INT8_MIN/2))	state |= PADEX_SUBSTICK_LEFT;
				if(padstatus[i].substickX>(INT8_MAX/2))	state |= PADEX_SUBSTICK_RIGHT;
				if(padstatus[i].substickY<(INT8_MIN/2))	state |= PADEX_SUBSTICK_DOWN;
				if(padstatus[i].substickY>(INT8_MAX/2))	state |= PADEX_SUBSTICK_UP;
				if(padstatus[i].triggerR>(UINT8_MAX/2))	state |= PADEX_TRIGGER_R;
				if(padstatus[i].triggerL>(UINT8_MAX/2))	state |= PADEX_TRIGGER_L;
				if(padstatus[i].analogA>(UINT8_MAX/2))	state |= PADEX_ANALOG_A;
				if(padstatus[i].analogB>(UINT8_MAX/2))	state |= PADEX_ANALOG_B;
			}

			state |= (state & (PAD_TRIGGER_R | PAD_TRIGGER_L)) << (__builtin_clz(PAD_TRIGGER_R | PAD_TRIGGER_L) - __builtin_clz(PADEX_TRIGGER_R | PADEX_TRIGGER_L));

			oldstate				= __pad_keys[i].state;
			__pad_keys[i].up		= ~state & oldstate;
			__pad_keys[i].down		= state & ~oldstate;
			__pad_keys[i].state		= state;
			__pad_keys[i].chan		= i;

			connected |= (1<<i);
			break;

		case PAD_ERR_NO_CONTROLLER:
			switch(SI_Probe(i)) {
			case SI_N64_CONTROLLER:
				N64_GetResponse(i,&n64status);

				switch(n64status.err) {
				case N64_ERR_READY:
					state = 0;
					if(n64status.button&N64_BUTTON_LEFT)	state |= PAD_BUTTON_LEFT;
					if(n64status.button&N64_BUTTON_RIGHT)	state |= PAD_BUTTON_RIGHT;
					if(n64status.button&N64_BUTTON_DOWN)	state |= PAD_BUTTON_DOWN;
					if(n64status.button&N64_BUTTON_UP)		state |= PAD_BUTTON_UP;
					if(n64status.button&N64_TRIGGER_Z)		state |= PAD_TRIGGER_Z;
					if(n64status.button&N64_TRIGGER_R)		state |= PAD_TRIGGER_R | PADEX_TRIGGER_R;
					if(n64status.button&N64_TRIGGER_L)		state |= PAD_TRIGGER_L | PADEX_TRIGGER_L;
					if(n64status.button&N64_BUTTON_A)		state |= PAD_BUTTON_A;
					if(n64status.button&N64_BUTTON_B)		state |= PAD_BUTTON_B;
					if(n64status.button&N64_BUTTON_START)	state |= PAD_BUTTON_START;
					if(n64status.button&N64_BUTTON_C_LEFT)	state |= PADEX_SUBSTICK_LEFT  | PADEX_BUTTON_C_LEFT;
					if(n64status.button&N64_BUTTON_C_RIGHT)	state |= PADEX_SUBSTICK_RIGHT | PADEX_BUTTON_C_RIGHT;
					if(n64status.button&N64_BUTTON_C_DOWN)	state |= PADEX_SUBSTICK_DOWN  | PADEX_BUTTON_C_DOWN;
					if(n64status.button&N64_BUTTON_C_UP)	state |= PADEX_SUBSTICK_UP    | PADEX_BUTTON_C_UP;
					if(n64status.stickX<(INT8_MIN/2))		state |= PADEX_STICK_LEFT;
					if(n64status.stickX>(INT8_MAX/2))		state |= PADEX_STICK_RIGHT;
					if(n64status.stickY<(INT8_MIN/2))		state |= PADEX_STICK_DOWN;
					if(n64status.stickY>(INT8_MAX/2))		state |= PADEX_STICK_UP;

					__pad_keys[i].stickX	= n64status.stickX;
					__pad_keys[i].stickY	= n64status.stickY;
					__pad_keys[i].substickX	= 0;
					__pad_keys[i].substickY	= 0;
					__pad_keys[i].triggerL	= 0;
					__pad_keys[i].triggerR	= 0;

					if(state&PADEX_SUBSTICK_LEFT)	__pad_keys[i].substickX -= INT8_MAX;
					if(state&PADEX_SUBSTICK_RIGHT)	__pad_keys[i].substickX += INT8_MAX;
					if(state&PADEX_SUBSTICK_DOWN)	__pad_keys[i].substickY -= INT8_MAX;
					if(state&PADEX_SUBSTICK_UP)		__pad_keys[i].substickY += INT8_MAX;
					if(state&PADEX_TRIGGER_R)		__pad_keys[i].triggerR += UINT8_MAX;
					if(state&PADEX_TRIGGER_L)		__pad_keys[i].triggerL += UINT8_MAX;

					oldstate				= __pad_keys[i].state;
					__pad_keys[i].up		= ~state & oldstate;
					__pad_keys[i].down		= state & ~oldstate;
					__pad_keys[i].state		= state;
					__pad_keys[i].chan		= i;

					connected |= (1<<i);
					break;

				case N64_ERR_NO_CONTROLLER:
					N64_EnablePolling(padBit);
					goto no_controller;
					break;

				default:
					goto not_ready;
					break;
				}
				break;

			case SI_GC_STEERING:
				SI_InitSteering();
				SI_ReadSteering(i,&steering);

				switch(steering.err) {
				case SI_STEERING_ERR_READY:
					state					= steering.button;
					__pad_keys[i].stickX	= steering.steering;
					__pad_keys[i].triggerL	= steering.left;
					__pad_keys[i].triggerR	= steering.right;
					__pad_keys[i].analogA	= steering.gas;
					__pad_keys[i].analogB	= steering.brake;

					if(steering.steering<(INT8_MIN/4))	state |= PADEX_STEERING_LEFT;
					if(steering.steering>(INT8_MAX/4))	state |= PADEX_STEERING_RIGHT;
					if(steering.right>(UINT8_MAX/2))	state |= PADEX_PADDLE_RIGHT;
					if(steering.left>(UINT8_MAX/2))		state |= PADEX_PADDLE_LEFT;
					if(steering.gas>(UINT8_MAX/2))		state |= PADEX_PEDAL_GAS;
					if(steering.brake>(UINT8_MAX/2))	state |= PADEX_PEDAL_BRAKE;

					state |= (state & (PAD_TRIGGER_R | PAD_TRIGGER_L)) << (__builtin_clz(PAD_TRIGGER_R | PAD_TRIGGER_L) - __builtin_clz(PADEX_TRIGGER_R | PADEX_TRIGGER_L));

					oldstate				= __pad_keys[i].state;
					__pad_keys[i].up		= ~state & oldstate;
					__pad_keys[i].down		= state & ~oldstate;
					__pad_keys[i].state		= state;
					__pad_keys[i].chan		= i;

					connected |= (1<<i);
					break;

				case SI_STEERING_ERR_NO_CONTROLLER:
					SI_ResetSteeringAsync(i,NULL);
					goto no_controller;
					break;

				default:
					goto not_ready;
					break;
				}
				break;

			case SI_GC_CONTROLLER:
			case SI_GC_WAVEBIRD:
				resetBits |= padBit;
no_controller:
			default:
				if(__pad_keys[i].chan!=-1) memset(&__pad_keys[i],0,sizeof(keyinput));
				__pad_keys[i].chan = -1;
				break;
			}
			break;
not_ready:
		default:
			__pad_keys[i].up = __pad_keys[i].down = 0;
			if(__pad_keys[i].chan!=-1) connected |= (1<<i);
			break;
		}
	}
#ifdef _PAD_DEBUG
	printf("resetBits = %08x\n",resetBits);
#endif
	if(resetBits) {
		PAD_Reset(resetBits);
	}
	return connected;
}

u32 PAD_ButtonsUp(s32 chan)
{
	if(chan<PAD_CHAN0 || chan>PAD_CHAN3 || __pad_keys[chan].chan==-1) return 0;
	return __pad_keys[chan].up;
}

u32 PAD_ButtonsDown(s32 chan)
{
	if(chan<PAD_CHAN0 || chan>PAD_CHAN3 || __pad_keys[chan].chan==-1) return 0;
	return __pad_keys[chan].down;
}

u32 PAD_ButtonsHeld(s32 chan)
{
	if(chan<PAD_CHAN0 || chan>PAD_CHAN3 || __pad_keys[chan].chan==-1) return 0;
	return __pad_keys[chan].state;
}

s8 PAD_SubStickX(s32 chan)
{
	if(chan<PAD_CHAN0 || chan>PAD_CHAN3 || __pad_keys[chan].chan==-1) return 0;
	return __pad_keys[chan].substickX;
}

s8 PAD_SubStickY(s32 chan)
{
	if(chan<PAD_CHAN0 || chan>PAD_CHAN3 || __pad_keys[chan].chan==-1) return 0;
	return __pad_keys[chan].substickY;
}

s8 PAD_StickX(s32 chan)
{
	if(chan<PAD_CHAN0 || chan>PAD_CHAN3 || __pad_keys[chan].chan==-1) return 0;
	return __pad_keys[chan].stickX;
}

s8 PAD_StickY(s32 chan)
{
	if(chan<PAD_CHAN0 || chan>PAD_CHAN3 || __pad_keys[chan].chan==-1) return 0;
	return __pad_keys[chan].stickY;
}

u8 PAD_TriggerL(s32 chan)
{
	if(chan<PAD_CHAN0 || chan>PAD_CHAN3 || __pad_keys[chan].chan==-1) return 0;
	return __pad_keys[chan].triggerL;
}

u8 PAD_TriggerR(s32 chan)
{
	if(chan<PAD_CHAN0 || chan>PAD_CHAN3 || __pad_keys[chan].chan==-1) return 0;
	return __pad_keys[chan].triggerR;
}

u8 PAD_AnalogA(s32 chan)
{
	if(chan<PAD_CHAN0 || chan>PAD_CHAN3 || __pad_keys[chan].chan==-1) return 0;
	return __pad_keys[chan].analogA;
}

u8 PAD_AnalogB(s32 chan)
{
	if(chan<PAD_CHAN0 || chan>PAD_CHAN3 || __pad_keys[chan].chan==-1) return 0;
	return __pad_keys[chan].analogB;
}
