/*-------------------------------------------------------------

exi.c -- EXI subsystem

Copyright (C) 2004 - 2025
Michael Wiedenbauer (shagkur)
Dave Murphy (WinterMute)
Extrems' Corner.org

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
#include <string.h>
#include <stdio.h>
#include "asm.h"
#include "irq.h"
#include "processor.h"
#include "system.h"
#include "cache.h"
#include "exi.h"
#include "lwp.h"
#include "lwp_threads.h"
#include "card_cmn.h"
#include "card_io.h"

//#define _EXI_DEBUG

#define EXI_LOCK_DEVS				32

#define EXI_MAX_CHANNELS			3
#define EXI_MAX_DEVICES				3

#define EXI_DEVICE0					0x0080
#define EXI_DEVICE1					0x0100
#define EXI_DEVICE2					0x0200

#define EXI_EXI_IRQ					0x0002
#define EXI_TC_IRQ					0x0008
#define EXI_EXT_IRQ					0x0800
#define EXI_EXT_BIT					0x1000

#define _SHIFTL(v, s, w)	\
    ((u32) (((u32)(v) & ((0x01 << (w)) - 1)) << (s)))
#define _SHIFTR(v, s, w)	\
    ((u32)(((u32)(v) >> (s)) & ((0x01 << (w)) - 1)))


struct _lck_dev {
	lwp_node node;
	u32 dev;
	EXICallback unlockcb;
};

typedef struct _exibus_priv {
	EXICallback CallbackEXI;
	EXICallback CallbackTC;
	EXICallback CallbackEXT;

	u32 flags;
	u32 imm_len;
	void *imm_buff;
	u32 lockeddev;
	u32 exi_id;
	u64 exi_idtime;
	u32 lck_cnt;
	u32 lckd_dev_bits;
	lwp_queue lckd_dev;
	lwpq_t syncqueue;
} exibus_priv;

static lwp_queue _lckdev_queue;
static struct _lck_dev lckdevs[EXI_LOCK_DEVS];
static exibus_priv eximap[EXI_MAX_CHANNELS];
static u64 last_exi_idtime[EXI_MAX_CHANNELS];

static u32 exi_id_serport1 = 0;

static u32 exi_uart_chan = EXI_CHANNEL_0;
static u32 exi_uart_dev = EXI_DEVICE_0;
static u32 exi_uart_barnacle_enabled = 0;
static u32 exi_uart_enabled = 0;

static void __exi_irq_handler(u32,frame_context *);
static void __tc_irq_handler(u32,frame_context *);
static void __ext_irq_handler(u32,frame_context *);

#if defined(HW_DOL)
	static vu32 (*const _exiReg)[5] = (u32(*)[])0xCC006800;
#elif defined(HW_RVL)
	static vu32 (*const _exiReg)[5] = (u32(*)[])0xCD006800;
#else
	#error HW model unknown.
#endif

static __inline__ void __exi_clearirqs(s32 nChn,u32 nEXIIrq,u32 nTCIrq,u32 nEXTIrq)
{
	u32 d;
#ifdef _EXI_DEBUG
	printf("__exi_clearirqs(%d,%d,%d,%d)\n",nChn,nEXIIrq,nTCIrq,nEXTIrq);
#endif
	d = (_exiReg[nChn][0]&~(EXI_EXI_IRQ|EXI_TC_IRQ|EXI_EXT_IRQ));
	if(nEXIIrq) d |= EXI_EXI_IRQ;
	if(nTCIrq) d |= EXI_TC_IRQ;
	if(nEXTIrq) d |= EXI_EXT_IRQ;
	_exiReg[nChn][0] = d;
}

static __inline__ void __exi_setinterrupts(s32 nChn,exibus_priv *exi)
{
	exibus_priv *pexi = &eximap[EXI_CHANNEL_2];
#ifdef _EXI_DEBUG
	printf("__exi_setinterrupts(%d,%p)\n",nChn,exi);
#endif
	if(nChn==EXI_CHANNEL_0) {
		__MaskIrq((IRQMASK(IRQ_EXI0_EXI)|IRQMASK(IRQ_EXI2_EXI)));
		if(!(exi->flags&EXI_FLAG_LOCKED) && (exi->CallbackEXI || pexi->CallbackEXI))
			__UnmaskIrq((IRQMASK(IRQ_EXI0_EXI)|IRQMASK(IRQ_EXI2_EXI)));
	} else if(nChn==EXI_CHANNEL_1) {
		__MaskIrq(IRQMASK(IRQ_EXI1_EXI));
		if(!(exi->flags&EXI_FLAG_LOCKED) && exi->CallbackEXI) __UnmaskIrq(IRQMASK(IRQ_EXI1_EXI));
	} else if(nChn==EXI_CHANNEL_2) {				//explicitly use of channel 2 only if debugger is attached.
		__MaskIrq(IRQMASK(IRQ_PI_DEBUG));
		if(!(exi->flags&EXI_FLAG_LOCKED) && IRQ_GetHandler(IRQ_PI_DEBUG)) __UnmaskIrq(IRQMASK(IRQ_PI_DEBUG));
	}
}

static void __exi_initmap(exibus_priv *exim)
{
	s32 i;
	exibus_priv *m;

	__lwp_queue_initialize(&_lckdev_queue,lckdevs,EXI_LOCK_DEVS,sizeof(struct _lck_dev));

	for(i=0;i<EXI_MAX_CHANNELS;i++) {
		m = &exim[i];
		m->CallbackEXI = NULL;
		m->CallbackTC = NULL;
		m->CallbackEXT = NULL;
		m->flags = 0;
		m->imm_len = 0;
		m->imm_buff = NULL;
		m->lockeddev = 0;
		m->exi_id = 0;
		m->exi_idtime = 0;
		m->lck_cnt = 0;
		m->lckd_dev_bits = 0;
		__lwp_queue_init_empty(&m->lckd_dev);
		m->syncqueue = LWP_TQUEUE_NULL;
	}
}

static s32 __exi_probe(s32 nChn)
{
	u64 time;
	s32 ret = 1;
	u32 level;
	u32 val;
	exibus_priv *exi = &eximap[nChn];
#ifdef _EXI_DEBUG
	printf("__exi_probe(%d)\n",nChn);
#endif
	if(nChn==EXI_CHANNEL_2) return ret;
	_CPU_ISR_Disable(level);
	val = _exiReg[nChn][0];
	if(!(exi->flags&EXI_FLAG_ATTACH)) {
		if(val&EXI_EXT_IRQ) {
			__exi_clearirqs(nChn,0,0,1);
			exi->exi_idtime = 0;
			last_exi_idtime[nChn] = 0;
		}
		if(_exiReg[nChn][0]&EXI_EXT_BIT) {
			time = __SYS_GetSystemTime();
			if(last_exi_idtime[nChn]==0) last_exi_idtime[nChn] = time;
			if((val=diff_usec(last_exi_idtime[nChn],time)+10)<30) ret = 0;
			else ret = 1;
#ifdef _EXI_DEBUG
			printf("val = %u, ret = %d, last_exi_idtime[chn] = %llu\n",val,ret,last_exi_idtime[nChn]);
#endif
			_CPU_ISR_Restore(level);
			return ret;
		} else {
			exi->exi_idtime = 0;
			last_exi_idtime[nChn] = 0;
			_CPU_ISR_Restore(level);
			return 0;
		}
	}

	if(!(_exiReg[nChn][0]&EXI_EXT_BIT) || (_exiReg[nChn][0]&EXI_EXT_IRQ)) {
		exi->exi_idtime = 0;
		last_exi_idtime[nChn] = 0;
		ret = 0;
	}
	_CPU_ISR_Restore(level);
	return ret;
}

static s32 __exi_attach(s32 nChn,EXICallback ext_cb)
{
	s32 ret;
	u32 level;
	exibus_priv *exi = &eximap[nChn];
#ifdef _EXI_DEBUG
	printf("__exi_attach(%d,%p)\n",nChn,ext_cb);
#endif
	_CPU_ISR_Disable(level);
	ret = 0;
	if(!(exi->flags&EXI_FLAG_ATTACH)) {
		if(__exi_probe(nChn)==1) {
			__exi_clearirqs(nChn,1,0,0);
			exi->CallbackEXT = ext_cb;
			__UnmaskIrq(((IRQMASK(IRQ_EXI0_EXT))>>(nChn*3)));
			exi->flags |= EXI_FLAG_ATTACH;
			ret = 1;
		}
	}
	_CPU_ISR_Restore(level);
	return ret;
}

s32 EXI_Lock(s32 nChn,s32 nDev,EXICallback unlockCB)
{
	u32 level;
	struct _lck_dev *lckd;
	exibus_priv *exi = &eximap[nChn];
#ifdef _EXI_DEBUG
	printf("EXI_Lock(%d,%d,%p)\n",nChn,nDev,unlockCB);
#endif
	_CPU_ISR_Disable(level);
	if(exi->flags&EXI_FLAG_LOCKED) {
		if(unlockCB && !(exi->lckd_dev_bits&(1<<nDev))) {
			lckd = (struct _lck_dev*)__lwp_queue_getI(&_lckdev_queue);
			if(lckd) {
				exi->lck_cnt++;
				exi->lckd_dev_bits |= (1<<nDev);
				lckd->dev = nDev;
				lckd->unlockcb = unlockCB;
				__lwp_queue_appendI(&exi->lckd_dev,&lckd->node);
			}
		}
		_CPU_ISR_Restore(level);
		return 0;
	}

	exi->lockeddev = nDev;
	exi->flags |= EXI_FLAG_LOCKED;
	__exi_setinterrupts(nChn,exi);

	_CPU_ISR_Restore(level);
	return 1;
}

s32 EXI_Unlock(s32 nChn)
{
	u32 level,dev;
	EXICallback cb;
	struct _lck_dev *lckd;
	exibus_priv *exi = &eximap[nChn];
#ifdef _EXI_DEBUG
	printf("EXI_Unlock(%d)\n",nChn);
#endif
	_CPU_ISR_Disable(level);
	if(!(exi->flags&EXI_FLAG_LOCKED)) {
		_CPU_ISR_Restore(level);
		return 0;
	}

	exi->flags &= ~EXI_FLAG_LOCKED;
	__exi_setinterrupts(nChn,exi);

	if(!exi->lck_cnt) {
		_CPU_ISR_Restore(level);
		return 1;
	}

	exi->lck_cnt--;
	lckd = (struct _lck_dev*)__lwp_queue_getI(&exi->lckd_dev);
	__lwp_queue_appendI(&_lckdev_queue,&lckd->node);

	cb = lckd->unlockcb;
	dev = lckd->dev;
	exi->lckd_dev_bits &= ~(1<<dev);
	if(cb) cb(nChn,dev);

	_CPU_ISR_Restore(level);
	return 1;
}

s32 EXI_Select(s32 nChn,s32 nDev,s32 nFrq)
{
	u32 val;
	u32 level;
	exibus_priv *exi = &eximap[nChn];
#ifdef _EXI_DEBUG
	printf("EXI_Select(%d,%d,%d)\n",nChn,nDev,nFrq);
#endif
	_CPU_ISR_Disable(level);

	if(exi->flags&EXI_FLAG_SELECT) {
#ifdef _EXI_DEBUG
		printf("EXI_Select(): allready selected.\n");
#endif
		_CPU_ISR_Restore(level);
		return 0;
	}

	if(nChn!=EXI_CHANNEL_2) {
		if(nDev==EXI_DEVICE_0 && !(exi->flags&EXI_FLAG_ATTACH)) {
			if(__exi_probe(nChn)==0) {
				_CPU_ISR_Restore(level);
				return 0;
			}
		}
		if(!(exi->flags&EXI_FLAG_LOCKED) || exi->lockeddev!=nDev) {
#ifdef _EXI_DEBUG
			printf("EXI_Select(): not locked or wrong dev(%d).\n",exi->lockeddev);
#endif
			_CPU_ISR_Restore(level);
			return 0;
		}
	}

	exi->flags |= EXI_FLAG_SELECT;
	val = _exiReg[nChn][0];
	val = (val&0x405)|(0x80<<nDev)|(nFrq<<4);
	_exiReg[nChn][0] = val;

	if(exi->flags&EXI_FLAG_ATTACH) {
		if(nChn==EXI_CHANNEL_0) __MaskIrq(IRQMASK(IRQ_EXI0_EXT));
		else if(nChn==EXI_CHANNEL_1) __MaskIrq(IRQMASK(IRQ_EXI1_EXT));
	}

	_CPU_ISR_Restore(level);
	return 1;
}

s32 EXI_SelectSD(s32 nChn,s32 nDev,s32 nFrq)
{
	u32 val;
	u32 level;
	exibus_priv *exi = &eximap[nChn];
#ifdef _EXI_DEBUG
	printf("EXI_SelectSD(%d,%d,%d)\n",nChn,nDev,nFrq);
#endif
	_CPU_ISR_Disable(level);

	if(exi->flags&EXI_FLAG_SELECT) {
#ifdef _EXI_DEBUG
		printf("EXI_SelectSD(): allready selected.\n");
#endif
		_CPU_ISR_Restore(level);
		return 0;
	}

	if(nChn!=EXI_CHANNEL_2) {
		if(nDev==EXI_DEVICE_0 && !(exi->flags&EXI_FLAG_ATTACH)) {
			if(EXI_Probe(nChn)==0) {
				_CPU_ISR_Restore(level);
				return 0;
			}
		}
		if(!(exi->flags&EXI_FLAG_LOCKED) || exi->lockeddev!=nDev) {
#ifdef _EXI_DEBUG
			printf("EXI_SelectSD(): not locked or wrong dev(%d).\n",exi->lockeddev);
#endif
			_CPU_ISR_Restore(level);
			return 0;
		}
	}

	exi->flags |= EXI_FLAG_SELECT;
	val = _exiReg[nChn][0];
	val = (val&0x405)|(nFrq<<4);
	_exiReg[nChn][0] = val;

	if(exi->flags&EXI_FLAG_ATTACH) {
		if(nChn==EXI_CHANNEL_0) __MaskIrq(IRQMASK(IRQ_EXI0_EXT));
		else if(nChn==EXI_CHANNEL_1) __MaskIrq(IRQMASK(IRQ_EXI1_EXT));
	}

	_CPU_ISR_Restore(level);
	return 1;
}

s32 EXI_Deselect(s32 nChn)
{
	u32 val;
	u32 level;
	exibus_priv *exi = &eximap[nChn];
#ifdef _EXI_DEBUG
	printf("EXI_Deselect(%d)\n",nChn);
#endif
	_CPU_ISR_Disable(level);

	if(!(exi->flags&EXI_FLAG_SELECT)) {
		_CPU_ISR_Restore(level);
		return 0;
	}

	exi->flags &= ~EXI_FLAG_SELECT;
	val = _exiReg[nChn][0];
	_exiReg[nChn][0] = (val&0x405);

	if(exi->flags&EXI_FLAG_ATTACH) {
		if(nChn==EXI_CHANNEL_0) __UnmaskIrq(IRQMASK(IRQ_EXI0_EXT));
		else if(nChn==EXI_CHANNEL_1) __UnmaskIrq(IRQMASK(IRQ_EXI1_EXT));
	}

	if(nChn!=EXI_CHANNEL_2 && val&EXI_DEVICE0) {
		if(__exi_probe(nChn)==0) {
			_CPU_ISR_Restore(level);
			return 0;
		}
	}
	_CPU_ISR_Restore(level);
	return 1;
}

s32 EXI_Sync(s32 nChn)
{
	u8 *buf;
	s32 ret;
	u32 level,len,val;
	exibus_priv *exi = &eximap[nChn];
#ifdef _EXI_DEBUG
	printf("EXI_Sync(%d)\n",nChn);
#endif
	while(_exiReg[nChn][3]&0x0001);

	_CPU_ISR_Disable(level);

	ret = 0;
	if(exi->flags&EXI_FLAG_SELECT && exi->flags&(EXI_FLAG_DMA|EXI_FLAG_IMM)) {
		if(exi->flags&EXI_FLAG_IMM) {
			buf = exi->imm_buff;
			len = exi->imm_len;
			if(len>0) {
				val = _exiReg[nChn][4];
				__stswx(buf,len,val);
			}
		}
		exi->flags &= ~(EXI_FLAG_DMA|EXI_FLAG_IMM);
		ret = 1;
	}
	_CPU_ISR_Restore(level);
	return ret;
}

static s32 __exi_synccallback(s32 nChn,s32 nDev)
{
	exibus_priv *exi = &eximap[nChn];
	LWP_ThreadBroadcast(exi->syncqueue);
	return 1;
}

static s32 __exi_syncex(s32 nChn)
{
	u32 level;
	exibus_priv *exi = &eximap[nChn];

	_CPU_ISR_Disable(level);
	if(exi->syncqueue==LWP_TQUEUE_NULL) {
		if(LWP_InitQueue(&exi->syncqueue)==-1) {
			_CPU_ISR_Restore(level);
			return 0;
		}
	}
	while(exi->flags&(EXI_FLAG_DMA|EXI_FLAG_IMM)) {
		LWP_ThreadSleep(exi->syncqueue);
	}
	_CPU_ISR_Restore(level);
	return 1;
}

s32 EXI_Imm(s32 nChn,void *pData,u32 nLen,u32 nMode,EXICallback tc_cb)
{
	u32 val;
	u32 level;
	exibus_priv *exi = &eximap[nChn];
#ifdef _EXI_DEBUG
	printf("EXI_Imm(%d,%p,%d,%d,%p)\n",nChn,pData,nLen,nMode,tc_cb);
#endif
	_CPU_ISR_Disable(level);

	if(exi->flags&(EXI_FLAG_DMA|EXI_FLAG_IMM) || !(exi->flags&EXI_FLAG_SELECT)) {
		_CPU_ISR_Restore(level);
		return 0;
	}

	exi->CallbackTC = tc_cb;
	if(tc_cb) {
		__exi_clearirqs(nChn,0,1,0);
		__UnmaskIrq(IRQMASK((IRQ_EXI0_TC+(nChn*3))));
	}
	exi->flags |= EXI_FLAG_IMM;

	exi->imm_buff = pData;
	exi->imm_len = nLen;
	val = 0xffffffff;
	if(nMode==EXI_WRITE) exi->imm_len = 0;
	if(nMode!=EXI_READ) val = __lswx(pData,nLen);
	_exiReg[nChn][4] = val;
	_exiReg[nChn][3] = (((nLen-1)&0x03)<<4)|((nMode&0x03)<<2)|0x01;

	_CPU_ISR_Restore(level);
	return 1;
}

s32 __attribute__((flatten)) EXI_ImmEx(s32 nChn,void *pData,u32 nLen,u32 nMode)
{
	u8 *buf = pData;
	u32 len;
#ifdef _EXI_DEBUG
	printf("EXI_ImmEx(%d,%p,%d,%d)\n",nChn,pData,nLen,nMode);
#endif
	while(nLen) {
		len = nLen;
		if(len>4) len = 4;

		if(!EXI_Imm(nChn,buf,len,nMode,NULL)) return 0;
		if(!EXI_Sync(nChn)) return 0;
		nLen -= len;
		buf += len;
	}
	return 1;
}

s32 EXI_Dma(s32 nChn,void *pData,u32 nLen,u32 nMode,EXICallback tc_cb)
{
	u32 level;
	exibus_priv *exi = &eximap[nChn];
#ifdef _EXI_DEBUG
	printf("EXI_Dma(%d,%p,%d,%d,%p)\n",nChn,pData,nLen,nMode,tc_cb);
#endif
	_CPU_ISR_Disable(level);

	if(exi->flags&(EXI_FLAG_DMA|EXI_FLAG_IMM) || !(exi->flags&EXI_FLAG_SELECT)) {
#ifdef _EXI_DEBUG
		printf("EXI_Dma(%04x): abort\n",exi->flags);
#endif
		_CPU_ISR_Restore(level);
		return 0;
	}
#ifdef _EXI_DEBUG
	printf("EXI_Dma(tccb: %p)\n",tc_cb);
#endif
	exi->CallbackTC = tc_cb;
	if(tc_cb) {
		__exi_clearirqs(nChn,0,1,0);
		__UnmaskIrq((IRQMASK((IRQ_EXI0_TC+(nChn*3)))));
	}
	exi->flags |= EXI_FLAG_DMA;

	_exiReg[nChn][1] = (u32)pData&0x1FFFFFE0;
	_exiReg[nChn][2] = nLen;
	_exiReg[nChn][3] = ((nMode&0x03)<<2)|0x03;

	_CPU_ISR_Restore(level);
	return 1;
}

s32 EXI_DmaEx(s32 nChn,void *pData,u32 nLen,u32 nMode)
{
	u32 roundlen;
	s32 missalign;
	s32 len = nLen;
	u8 *ptr = pData;

	if(!ptr || len<=0) return 0;

	missalign = -((u32)ptr)&0x1f;
	if((len-missalign)<32) return EXI_ImmEx(nChn,ptr,len,nMode);

	if(missalign>0) {
		if(!EXI_ImmEx(nChn,ptr,missalign,nMode)) return 0;
		len -= missalign;
		ptr += missalign;
	}

	roundlen = (len&~0x1f);
	if(nMode==EXI_READ) DCInvalidateRange(ptr,roundlen);
	else DCStoreRange(ptr,roundlen);
	if(!__lwp_isr_in_progress()) {
		if(!EXI_Dma(nChn,ptr,roundlen,nMode,__exi_synccallback)) return 0;
		if(!__exi_syncex(nChn)) return 0;
	} else {
		if(!EXI_Dma(nChn,ptr,roundlen,nMode,NULL)) return 0;
		if(!EXI_Sync(nChn)) return 0;
	}

	len -= roundlen;
	ptr += roundlen;
	if(len>0) return EXI_ImmEx(nChn,ptr,len,nMode);

	return 1;
}

s32 EXI_GetState(s32 nChn)
{
	exibus_priv *exi = &eximap[nChn];
	return exi->flags;
}

static s32 __unlocked_handler(s32 nChn,s32 nDev)
{
	u32 nId;
#ifdef _EXI_DEBUG
	printf("__unlocked_handler(%d,%d)\n",nChn,nDev);
#endif
	EXI_GetID(nChn,nDev,&nId);
	return 1;
}

s32 EXI_GetID(s32 nChn,s32 nDev,u32 *nId)
{
	u64 idtime = 0;
	s32 ret,lck;
	u32 level,cnt,id;
	exibus_priv *exi = &eximap[nChn];

	if(nChn==EXI_CHANNEL_0 && nDev==EXI_DEVICE_2 && exi_id_serport1!=0) {
		*nId = exi_id_serport1;
		return 1;
	}
#ifdef _EXI_DEBUG
	printf("EXI_GetID(exi_id = %d)\n",exi->exi_id);
#endif
	if(nChn<EXI_CHANNEL_2 && nDev==EXI_DEVICE_0) {
		if(__exi_probe(nChn)==0) return 0;
		if(exi->exi_idtime==last_exi_idtime[nChn]) {
#ifdef _EXI_DEBUG
			printf("EXI_GetID(exi_id = %d)\n",exi->exi_id);
#endif
			*nId = exi->exi_id;
			return 1;
		}
#ifdef _EXI_DEBUG
		printf("EXI_GetID(setting interrupts,%08x)\n",exi->flags);
#endif
		if(__exi_attach(nChn,NULL)==0) return 0;
		idtime = last_exi_idtime[nChn];
	} else if(nChn==EXI_CHANNEL_2 && nDev==EXI_DEVICE_0 && exi->exi_id!=0) {
		*nId = exi->exi_id;
		return 1;
	}
#ifdef _EXI_DEBUG
	printf("EXI_GetID(interrupts set)\n");
#endif
	lck = 0;
	if(nChn<EXI_CHANNEL_2 && nDev==EXI_DEVICE_0) lck = 1;

	if(lck) ret = EXI_Lock(nChn,nDev,__unlocked_handler);
	else ret = EXI_Lock(nChn,nDev,NULL);

	if(ret) {
		id = 0xffffffff;
		cnt = 0;
		while(cnt<4) {
			if((ret=EXI_GetIDEx(nChn,nDev,nId))==0) break;
			if(id==*nId) break;
			id = *nId;
			cnt++;
		}
		EXI_Unlock(nChn);
	}

	if(nChn<EXI_CHANNEL_2 && nDev==EXI_DEVICE_0) {
		EXI_Detach(nChn);

		if(ret) {
			_CPU_ISR_Disable(level);
			if(idtime==last_exi_idtime[nChn]) {
				exi->exi_idtime = idtime;
				exi->exi_id = *nId;
			} else
				ret = 0;
			_CPU_ISR_Restore(level);
		}
#ifdef _EXI_DEBUG
		printf("EXI_GetID(exi_id = %d)\n",exi->exi_id);
#endif
	} else if(nChn==EXI_CHANNEL_2 && nDev==EXI_DEVICE_0 && ret)
		exi->exi_id = *nId;
	return ret;
}

s32 EXI_GetIDEx(s32 nChn,s32 nDev,u32 *nId)
{
	s32 ret;
	u32 reg;

	if(nDev==sdgecko_getDevice(nChn)
		&& sdgecko_isInitialized(nChn)) {
		*nId = 0xffffffff;
		return 1;
	}

	ret = 0;
	reg = 0;
	if(EXI_Select(nChn,nDev,EXI_SPEED1MHZ)==0) return 0;
	if(EXI_ImmEx(nChn,&reg,2,EXI_WRITE)==0) ret |= 0x01;
	if(EXI_ImmEx(nChn,nId,4,EXI_READ)==0) ret |= 0x02;
	if(EXI_Deselect(nChn)==0) ret |= 0x04;

	if(ret) return 0;
	return 1;
}

s32 EXI_GetType(s32 nChn,s32 nDev,u32 *nType)
{
	u32 nId;
	s32 ret;

	if((ret=EXI_GetID(nChn,nDev,&nId))==0) return ret;

	switch(nId&~0xff) {
		case 0x04020100:
		case 0x04020200:
		case 0x04020300:
		case 0x04060000:
		case 0x49444500:
			*nType = nId&~0xff;
			return ret;
	}
	switch(nId&~0xffff) {
		case 0:
			if(nId&0x3803) break;
			switch(nId&0xfc) {
				case EXI_MEMCARD59:
				case EXI_MEMCARD123:
				case EXI_MEMCARD251:
				case EXI_MEMCARD507:
				case EXI_MEMCARD1019:
				case EXI_MEMCARD2043:
					*nType = nId&0xfc;
					return ret;
			}
			break;
		case 0x05070000:
			*nType = nId&~0xffff;
			return ret;
	}
	*nType = nId;
	return ret;
}

char *EXI_GetTypeString(u32 nType)
{
	switch(nType) {
		case EXI_MEMCARD59:
			return "Memory Card 59";
		case EXI_MEMCARD123:
			return "Memory Card 123";
		case EXI_MEMCARD251:
			return "Memory Card 251";
		case EXI_MEMCARD507:
			return "Memory Card 507";
		case EXI_MEMCARD1019:
			return "Memory Card 1019";
		case EXI_MEMCARD2043:
			return "Memory Card 2043";
		case 0x01010000:
			return "USB Adapter";
		case 0x01020000:
			return "GDEV";
		case 0x02020000:
			return "Modem";
		case 0x03000000:
			return "WIZnet";
		case 0x03010000:
			return "Marlin";
		case 0x04120000:
			return "AD16";
		case 0x04040404:
			return "RS232C";
		case 0x80000004:
		case 0x80000008:
		case 0x80000010:
		case 0x80000020:
		case 0x80000040:
		case 0x80000080:
			return "Net Card";
		case 0x04220001:
			return "Artist Ether";
		case 0x04220000:
		case 0x04020100:
		case 0x04020200:
		case 0x04020300:
			return "Broadband Adapter";
		case 0x04060000:
		case 0x0a000000:
			return "Mic";
		case 0x04130000:
			return "Stream Hanger";
		case 0x05070000:
			return "IS-DOL-VIEWER";
		case 0x49444500:
			return "IDE-EXI";
		case 0xfa050000:
			return "ENC28J60";
		default:
			return "Unknown";
	}
}

s32 EXI_Attach(s32 nChn,EXICallback ext_cb)
{
	s32 ret;
	u32 level;
	exibus_priv *exi = &eximap[nChn];
#ifdef _EXI_DEBUG
	printf("EXI_Attach(%d)\n",nChn);
#endif
	EXI_Probe(nChn);

	_CPU_ISR_Disable(level);
	if(exi->exi_idtime) {
		ret = __exi_attach(nChn,ext_cb);
	} else
		ret = 0;
	_CPU_ISR_Restore(level);
	return ret;
}

s32 EXI_Detach(s32 nChn)
{
	u32 level;
	s32 ret = 1;
	exibus_priv *exi = &eximap[nChn];
#ifdef _EXI_DEBUG
	printf("EXI_Detach(%d)\n",nChn);
#endif
	_CPU_ISR_Disable(level);
	if(exi->flags&EXI_FLAG_ATTACH) {
		if(exi->flags&EXI_FLAG_LOCKED && exi->lockeddev!=EXI_DEVICE_0) ret = 0;
		else {
			exi->flags &= ~EXI_FLAG_ATTACH;
			__MaskIrq(((IRQMASK(IRQ_EXI0_EXI)|IRQMASK(IRQ_EXI0_TC)|IRQMASK(IRQ_EXI0_EXT))>>(nChn*3)));
		}
	}
	_CPU_ISR_Restore(level);
	return ret;
}

EXICallback EXI_RegisterEXICallback(s32 nChn,EXICallback exi_cb)
{
	u32 level;
	EXICallback old = NULL;
	exibus_priv *exi = &eximap[nChn];
#ifdef _EXI_DEBUG
	printf("EXI_RegisterEXICallback(%d,%p)\n",nChn,exi_cb);
#endif
	_CPU_ISR_Disable(level);
	old = exi->CallbackEXI;
	exi->CallbackEXI = exi_cb;
	if(nChn==EXI_CHANNEL_2) __exi_setinterrupts(EXI_CHANNEL_0,&eximap[EXI_CHANNEL_0]);
	else __exi_setinterrupts(nChn,exi);
	_CPU_ISR_Restore(level);
	return old;
}

s32 EXI_Probe(s32 nChn)
{
	s32 ret;
	u32 id;
	exibus_priv *exi = &eximap[nChn];
#ifdef _EXI_DEBUG
	printf("EXI_Probe(%d)\n",nChn);
#endif
	if((ret=__exi_probe(nChn))==1) {
		if(exi->exi_idtime==0) {
			if(EXI_GetID(nChn,EXI_DEVICE_0,&id)==0) ret = 0;
		}
	}
	return ret;
}

s32 EXI_ProbeEx(s32 nChn)
{
	if(EXI_Probe(nChn)==1) return 1;
	if(last_exi_idtime[nChn]==0) return -1;
	return 0;
}

void EXI_ProbeReset(void)
{
	last_exi_idtime[0] = 0;
	last_exi_idtime[1] = 0;

	eximap[0].exi_idtime = 0;
	eximap[1].exi_idtime = 0;
	eximap[2].exi_id = 0;

	__exi_probe(0);
	__exi_probe(1);
}

void __exi_init(void)
{
#ifdef _EXI_DEBUG
	printf("__exi_init(): init expansion system.\n");
#endif
	while(_exiReg[EXI_CHANNEL_0][3]&0x0001);
	while(_exiReg[EXI_CHANNEL_1][3]&0x0001);
	while(_exiReg[EXI_CHANNEL_2][3]&0x0001);

	__MaskIrq(IM_EXI);

	_exiReg[EXI_CHANNEL_0][0] = 0;
	_exiReg[EXI_CHANNEL_1][0] = 0;
	_exiReg[EXI_CHANNEL_2][0] = 0;

	_exiReg[EXI_CHANNEL_0][0] = 0x2000;

	__exi_initmap(eximap);

	IRQ_Request(IRQ_EXI0_EXI,__exi_irq_handler);
	IRQ_Request(IRQ_EXI0_TC,__tc_irq_handler);
	IRQ_Request(IRQ_EXI0_EXT,__ext_irq_handler);
	IRQ_Request(IRQ_EXI1_EXI,__exi_irq_handler);
	IRQ_Request(IRQ_EXI1_TC,__tc_irq_handler);
	IRQ_Request(IRQ_EXI1_EXT,__ext_irq_handler);
	IRQ_Request(IRQ_EXI2_EXI,__exi_irq_handler);
	IRQ_Request(IRQ_EXI2_TC,__tc_irq_handler);

	EXI_GetID(EXI_CHANNEL_0,EXI_DEVICE_2,&exi_id_serport1);

	EXI_ProbeReset();
}

void __exi_irq_handler(u32 nIrq,frame_context *pCtx)
{
	u32 chan,dev;
	exibus_priv *exi = NULL;
	const u32 fact = 0x55555556;

	chan = ((fact*(nIrq-IRQ_EXI0_EXI))>>1)&0x0f;
	dev = _SHIFTR((_exiReg[chan][0]&0x380),8,2);

	exi = &eximap[chan];
	__exi_clearirqs(chan,1,0,0);

	if(!exi->CallbackEXI) return;
#ifdef _EXI_DEBUG
	printf("__exi_irq_handler(%p)\n",exi->CallbackEXI);
#endif
	exi->CallbackEXI(chan,dev);
}

void __tc_irq_handler(u32 nIrq,frame_context *pCtx)
{
	u32 len,val,chan,dev;
	EXICallback tccb;
	void *buf = NULL;
	exibus_priv *exi = NULL;
	const u32 fact = 0x55555556;

	chan = ((fact*(nIrq-IRQ_EXI0_TC))>>1)&0x0f;
	dev = _SHIFTR((_exiReg[chan][0]&0x380),8,2);

	exi = &eximap[chan];
	__MaskIrq(IRQMASK(nIrq));
	__exi_clearirqs(chan,0,1,0);

	tccb = exi->CallbackTC;
#ifdef _EXI_DEBUG
	printf("__tc_irq_handler(%p)\n",tccb);
#endif
	if(!tccb) return;

	exi->CallbackTC = NULL;
	if(exi->flags&(EXI_FLAG_DMA|EXI_FLAG_IMM)) {
		if(exi->flags&EXI_FLAG_IMM) {
			buf = exi->imm_buff;
			len = exi->imm_len;
			if(len>0) {
				val = _exiReg[chan][4];
				__stswx(buf,len,val);
			}
		}
		exi->flags &= ~(EXI_FLAG_DMA|EXI_FLAG_IMM);
	}
	tccb(chan,dev);
}

void __ext_irq_handler(u32 nIrq,frame_context *pCtx)
{

	u32 chan,dev;
	exibus_priv *exi = NULL;
	const u32 fact = 0x55555556;

	chan = ((fact*(nIrq-IRQ_EXI0_EXT))>>1)&0x0f;
	dev = _SHIFTR((_exiReg[chan][0]&0x380),8,2);

	exi = &eximap[chan];
	__MaskIrq(IRQMASK(nIrq));
	__exi_clearirqs(chan,0,0,1);

	exi->flags &= ~EXI_FLAG_ATTACH;
	if(exi->CallbackEXT) exi->CallbackEXT(chan,dev);
#ifdef _EXI_DEBUG
	printf("__ext_irq_handler(%p)\n",exi->CallbackEXT);
#endif
}


/* EXI UART stuff */
static s32 __probebarnacle(s32 chn,u32 dev,u32 *rev)
{
	u32 ret,reg;

	if(chn!=EXI_CHANNEL_2 && dev==EXI_DEVICE_0) {
		if(EXI_Attach(chn,NULL)==0) return 0;
	}

	ret = 0;
	if(EXI_Lock(chn,dev,NULL)==1) {
		if(EXI_Select(chn,dev,EXI_SPEED1MHZ)==1) {
			reg = 0x20011300;
			if(EXI_ImmEx(chn,&reg,sizeof(u32),EXI_WRITE)==0) ret |= 0x01;
			if(EXI_ImmEx(chn,rev,sizeof(u32),EXI_READ)==0) ret |= 0x02;
			if(EXI_Deselect(chn)==0) ret |= 0x04;
		}
		EXI_Unlock(chn);
	}

	if(chn!=EXI_CHANNEL_2 && dev==EXI_DEVICE_0) EXI_Detach(chn);

	if(ret) return 0;
	if(*rev==0xffffffff) return 0;

	return 1;
}

static s32 __queuelength(void)
{
	u32 reg;
	u8 len = 0;

	if(EXI_Select(exi_uart_chan,exi_uart_dev,EXI_SPEED8MHZ)==0) return -1;

	reg = 0x20010000;
	EXI_ImmEx(exi_uart_chan,&reg,sizeof(u32),EXI_WRITE);
	EXI_ImmEx(exi_uart_chan,&len,sizeof(u8),EXI_READ);

	EXI_Deselect(exi_uart_chan);

	return (16-len);
}

void __SYS_EnableBarnacle(s32 chn,u32 dev)
{
	u32 id,rev;

	if(EXI_GetID(chn,dev,&id)==0) return;

	if(id==0x01020000 || id==0x0004 || id==0x80000010 || id==0x80000008
		|| id==0x80000004 || id==0xffffffff || id==0x80000020 || id==0x0020
		|| id==0x0010 || id==0x0008 || id==0x01010000 || id==0x04040404
		|| id==0x04020100 || id==0x03010000 || id==0x02020000
		|| id==0x04020300 || id==0x04020200 || id==0x04130000
		|| id==0x04120000 || id==0x04060000 || id==0x04220000) return;

	if(__probebarnacle(chn,dev,&rev)==0) return;

	exi_uart_chan = chn;
	exi_uart_dev = dev;
	exi_uart_barnacle_enabled = 0xa5ff005a;
	exi_uart_enabled = 0xa5ff005a;
}

s32 InitializeUART(void)
{
	if(exi_uart_barnacle_enabled==0xa5ff005a) return 0;
	if((SYS_GetConsoleType()&SYS_CONSOLE_MASK)!=SYS_CONSOLE_DEVELOPMENT) {
		exi_uart_enabled = 0;
		return 2;
	}

	exi_uart_chan = EXI_CHANNEL_0;
	exi_uart_dev = EXI_DEVICE_1;
	exi_uart_enabled = 0xa5ff005a;
	return 0;
}

s32 WriteUARTN(void *buf,u32 len)
{
	u8 *ptr;
	u32 reg;
	s32 ret,qlen,cnt;

	if(exi_uart_enabled!=0xa5ff005a) return 2;
	if(EXI_Lock(exi_uart_chan,exi_uart_dev,NULL)==0) return 0;

	ptr = buf;
	while((ptr-(u8*)buf)<len) {
		if(*ptr=='\n') *ptr = '\r';
		ptr++;
	}

	ret = 0;
	ptr = buf;
	while(len) {
		if((qlen=__queuelength())<0) {
			ret = 3;
			break;
		} else if(qlen>=12 || qlen>=len) {
			if(EXI_Select(exi_uart_chan,exi_uart_dev,EXI_SPEED8MHZ)==0) {
				ret = 3;
				break;
			}

			reg = 0xa0010000;
			EXI_ImmEx(exi_uart_chan,&reg,sizeof(u32),EXI_WRITE);

			cnt = qlen<len?qlen:len;
			EXI_ImmEx(exi_uart_chan,ptr,cnt,EXI_WRITE);
			len -= cnt;
			ptr += cnt;

			EXI_Deselect(exi_uart_chan);
		}
	}

	EXI_Unlock(exi_uart_chan);
	return ret;
}
