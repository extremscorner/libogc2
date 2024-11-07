#ifndef __OGC_LWP_WATCHDOG_H__
#define __OGC_LWP_WATCHDOG_H__

#include <gctypes.h>
#include "lwp_queue.h"
#include "timesupp.h"

#define LWP_WD_INACTIVE				0
#define LWP_WD_INSERTED				1
#define LWP_WD_ACTIVE				2
#define LWP_WD_REMOVE				3

#define LWP_WD_FORWARD				0
#define LWP_WD_BACKWARD				1

#define LWP_WD_NOTIMEOUT			0

#define LWP_WD_ABS(x)				((s64)(x)>0?(s64)(x):-((s64)(x)))

#ifdef __cplusplus
extern "C" {
#endif

extern vu32 _wd_sync_level;
extern vu32 _wd_sync_count;
extern u32 _wd_ticks_since_boot;

extern lwp_queue _wd_ticks_queue;

typedef void (*wd_service_routine)(void *);

typedef struct _wdcntrl {
	lwp_node node;
	u64 start;
	u32 id;
	u32 state;
	u64 fire;
	wd_service_routine routine;
	void *usr_data;
} wd_cntrl;

void __lwp_watchdog_init();
void __lwp_watchdog_settimer(wd_cntrl *wd);
void __lwp_wd_insert(lwp_queue *header,wd_cntrl *wd);
u32 __lwp_wd_remove(lwp_queue *header,wd_cntrl *wd);
void __lwp_wd_tickle(lwp_queue *queue);
void __lwp_wd_adjust(lwp_queue *queue,u32 dir,s64 interval);

#ifdef LIBOGC_INTERNAL
#include <libogc/lwp_watchdog.inl>
#endif

#ifdef __cplusplus
	}
#endif

#endif
