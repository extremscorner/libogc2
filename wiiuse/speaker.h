#ifndef SPEAKER_H_INCLUDED
#define SPEAKER_H_INCLUDED

#include "wiiuse_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _wencstatus
{
	s32 predictor;
	s16 step_index;
	s32 step;
	s32 prev_sample;
	s16 sample1;
	s16 sample2;
	s32 coeff1;
	s32 coeff2;
	s32 idelta;
} WENCStatus;

u8 wencdata(WENCStatus *info,s16 sample);
void set_speakervol(struct wiimote_t *wm,ubyte vol);

#ifdef __cplusplus
}
#endif

#endif
