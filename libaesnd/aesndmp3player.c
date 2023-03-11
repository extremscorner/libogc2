/***************************************************************************
 *   Low Level MAD Library Implementation for the GameCube                 *
 *   By Daniel "nForce" Bloemendal                                         *
 *   nForce@Sympatico.ca                                                   *
 ***************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <gccore.h>
#include <ogcsys.h>
#include <mad.h>

#include "aesndlib.h"
#include "mp3player.h"

static u32 mp3volume = 256;
static AESNDPB *mp3voice = NULL;

#define STACKSIZE				(32768)

#define INPUT_BUFFER_SIZE		(32768)
#define NUM_FRAMES				12

static u8 InputBuffer[INPUT_BUFFER_SIZE+MAD_BUFFER_GUARD];
static struct mad_pcm OutputFrame[NUM_FRAMES];
static s16 OutputBuffer[sizeof(OutputFrame->samples)/sizeof(OutputFrame->samples[0][0])];

static u32 init_done = 0;
static BOOL thr_running = FALSE;
static void *mp3cb_data = NULL;

static void* StreamPlay(void *);
static u8 StreamPlay_Stack[STACKSIZE] ATTRIBUTE_ALIGN(8);
static lwp_t hStreamPlay = LWP_THREAD_NULL;
static mqbox_t thInputQueue = MQ_BOX_NULL;
static mqbox_t thOutputQueue = MQ_BOX_NULL;

static s32 (*mp3read)(void*,void *,s32);
static void (*mp3filterfunc)(struct mad_stream *,struct mad_frame *);

static void DataTransferCallback(AESNDPB *pb,u32 state);

struct _rambuffer
{
	const void *buf_addr;
	s32 len,pos;
} rambuffer;

static s32 _mp3ramcopy(void *usr_data,void *buffer,s32 len)
{
	struct _rambuffer *ram = (struct _rambuffer*)usr_data;
	const void *ptr = ((u8*)ram->buf_addr+ram->pos);

	if((ram->pos+len)>ram->len) {
		len = (ram->len - ram->pos);
	}
	if(len<=0) return 0;

	memcpy(buffer,ptr,len);
	ram->pos += len;

	return len;
}

void MP3Player_Init(void)
{
	if(!init_done) {
		init_done = 1;
		AESND_Init();
		mp3voice = AESND_AllocateVoice(DataTransferCallback);
		AESND_SetVoiceStream(mp3voice,true);
	}
}

s32 MP3Player_PlayBuffer(const void *buffer,s32 len,void (*filterfunc)(struct mad_stream *,struct mad_frame *))
{
	if(thr_running==TRUE) return -1;

	rambuffer.buf_addr = buffer;
	rambuffer.len = len;
	rambuffer.pos = 0;

	mp3cb_data = &rambuffer;
	mp3read = _mp3ramcopy;
	mp3filterfunc = filterfunc;
	if(LWP_CreateThread(&hStreamPlay,StreamPlay,NULL,StreamPlay_Stack,STACKSIZE,80)<0) {
		return -1;
	}
	return 0;
}

s32 MP3Player_PlayFile(void *cb_data,s32 (*reader)(void *,void *,s32),void (*filterfunc)(struct mad_stream *,struct mad_frame *))
{
	if(thr_running==TRUE) return -1;

	mp3cb_data = cb_data;
	mp3read = reader;
	mp3filterfunc = filterfunc;
	if(LWP_CreateThread(&hStreamPlay,StreamPlay,NULL,StreamPlay_Stack,STACKSIZE,80)<0) {
		return -1;
	}
	return 0;
}

void MP3Player_Stop(void)
{
	if(thr_running==FALSE) return;

	thr_running = FALSE;
	LWP_JoinThread(hStreamPlay,NULL);
}

BOOL MP3Player_IsPlaying(void)
{
	return thr_running;
}

static void *StreamPlay(void *arg)
{
	s32 i;
	BOOL atend;
	u8 *GuardPtr = NULL;
	struct mad_stream Stream;
	struct mad_frame Frame;
	struct mad_synth Synth;
	struct mad_pcm *Pcm;
	mad_timer_t Timer;

	thr_running = TRUE;

	MQ_Init(&thOutputQueue,NUM_FRAMES);
	MQ_Init(&thInputQueue,NUM_FRAMES);

	for(i=0;i<NUM_FRAMES;i++)
		MQ_Send(thInputQueue,(mqmsg_t)&OutputFrame[i],MQ_MSG_BLOCK);

	mad_stream_init(&Stream);
	mad_frame_init(&Frame);
	mad_synth_init(&Synth);
	mad_timer_reset(&Timer);

	atend = FALSE;
	while(atend==FALSE && thr_running==TRUE) {
		if(Stream.buffer==NULL || Stream.error==MAD_ERROR_BUFLEN) {
			u8 *ReadStart;
			s32 ReadSize, Remaining;

			if(Stream.next_frame!=NULL) {
				Remaining = Stream.bufend - Stream.next_frame;
				memmove(InputBuffer,Stream.next_frame,Remaining);
				ReadStart = InputBuffer + Remaining;
				ReadSize = INPUT_BUFFER_SIZE - Remaining;
			} else {
				ReadSize = INPUT_BUFFER_SIZE;
				ReadStart = InputBuffer;
				Remaining = 0;
			}

			ReadSize = mp3read(mp3cb_data,ReadStart,ReadSize);
			if(ReadSize<=0) {
				GuardPtr = ReadStart;
				memset(GuardPtr,0,MAD_BUFFER_GUARD);
				ReadSize = MAD_BUFFER_GUARD;
				atend = TRUE;
			}

			mad_stream_buffer(&Stream,InputBuffer,(ReadSize + Remaining));
			//Stream.error = 0;
		}

		while (!mad_frame_decode(&Frame,&Stream) && thr_running==TRUE) {
			if(mp3filterfunc)
				mp3filterfunc(&Stream,&Frame);

			mad_timer_add(&Timer,Frame.header.duration);
			mad_synth_frame(&Synth,&Frame);

			if(MQ_Receive(thInputQueue,(mqmsg_t)&Pcm,MQ_MSG_BLOCK) && Pcm!=NULL) {
				memcpy(Pcm,&Synth.pcm,sizeof(struct mad_pcm));
				MQ_Send(thOutputQueue,(mqmsg_t)Pcm,MQ_MSG_BLOCK);

				AESND_SetVoiceStop(mp3voice,false);
			}
		}

		if(MAD_RECOVERABLE(Stream.error)) {
		  if(Stream.error!=MAD_ERROR_LOSTSYNC
			|| Stream.this_frame!=GuardPtr) continue;
		} else {
			if(Stream.error!=MAD_ERROR_BUFLEN) break;
		}
	}

	mad_synth_finish(&Synth);
	mad_frame_finish(&Frame);
	mad_stream_finish(&Stream);

	if(thr_running==TRUE)
		MQ_Send(thOutputQueue,(mqmsg_t)NULL,MQ_MSG_BLOCK);
	else MQ_Jam(thOutputQueue,(mqmsg_t)NULL,MQ_MSG_BLOCK);

	while(MQ_Receive(thInputQueue,(mqmsg_t)&Pcm,MQ_MSG_BLOCK) && Pcm!=NULL);

	MQ_Close(thInputQueue);
	MQ_Close(thOutputQueue);

	thr_running = FALSE;

	return 0;
}

static __inline__ s16 FixedToShort(mad_fixed_t Fixed)
{
	/* Clipping */
	if(Fixed>=MAD_F_ONE)
		return(SHRT_MAX);
	if(Fixed<=-MAD_F_ONE)
		return(-SHRT_MAX);

	Fixed=Fixed>>(MAD_F_FRACBITS-15);
	return((s16)Fixed);
}

static void Interleave(struct mad_pcm *Pcm,s16 *buf)
{
	s32 i;

	if(Pcm->channels==2) {
		for(i=0;i<Pcm->length;i++) {
			*buf++ = FixedToShort(Pcm->samples[0][i]);
			*buf++ = FixedToShort(Pcm->samples[1][i]);
		}
	} else {
		for(i=0;i<Pcm->length;i++)
			*buf++ = FixedToShort(Pcm->samples[0][i]);
	}
}

static void DataTransferCallback(AESNDPB *pb,u32 state)
{
	struct mad_pcm *Pcm;

	switch(state) {
		case VOICE_STATE_STOPPED:
		case VOICE_STATE_RUNNING:
			break;
		case VOICE_STATE_STREAM:
			if(MQ_Receive(thOutputQueue,(mqmsg_t)&Pcm,MQ_MSG_NOBLOCK)) {
				if(Pcm!=NULL) {
					Interleave(Pcm,OutputBuffer);
					AESND_SetVoiceFormat(pb,(Pcm->channels==2)?VOICE_STEREO16:VOICE_MONO16);
					AESND_SetVoiceFrequency(pb,Pcm->samplerate);
					AESND_SetVoiceVolume(pb,mp3volume,mp3volume);
					AESND_SetVoiceBuffer(pb,OutputBuffer,(Pcm->length*Pcm->channels*sizeof(s16)));
				} else {
					AESND_SetVoiceStop(pb,true);
				}
				MQ_Send(thInputQueue,(mqmsg_t)Pcm,MQ_MSG_NOBLOCK);
			}
			break;
	}
}

void MP3Player_Volume(u32 volume)
{
	if(volume>256) volume = 256;

	mp3volume = volume;
}
