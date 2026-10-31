/* Sound: id's mixer (snd_dma.c, snd_mix.c) into a ring here, 22050 Hz
   16-bit stereo, and from it into maxsdk's PCM stream (mx/snd.h: audsrv,
   which plays it at 48 kHz). SNDDMA_Submit sends what the mixer has painted
   since the last call (up to paintedtime), as far as the stream has room;
   SNDDMA_GetDMAPos reports what has been sent as played. The sound plays
   the stream's ring later (about 100 ms). */

#include <mx/snd.h>

#include "quakedef.h"

#define RATE 22050
#define FRAMES 8192 /* the ring: 0.37 s, a power of two */

extern int paintedtime;

static s16 ring[FRAMES * 2] __attribute__((aligned(64)));
static int sent; /* frames sent to the stream, as paintedtime counts them */

qboolean SNDDMA_Init(void)
{
	if (mx_snd_init() < 0 || mx_snd_stream_format(RATE, 16, 2) < 0) {
		Con_Printf("SNDDMA_Init: no audsrv\n");
		return false;
	}
	shm = &sn;
	shm->splitbuffer = 0;
	shm->channels = 2;
	shm->samplebits = 16;
	shm->speed = RATE;
	shm->samples = FRAMES * 2;
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	shm->buffer = (unsigned char *)ring;
	sent = 0;
	Con_Printf("Sound: %d Hz 16-bit stereo through audsrv\n", RATE);
	return true;
}

int SNDDMA_GetDMAPos(void)
{
	shm->samplepos = (sent * 2) & (FRAMES * 2 - 1);
	return shm->samplepos;
}

void SNDDMA_Submit(void)
{
	if (paintedtime < sent) /* snd_dma.c chopped its counters */
		sent = paintedtime;
	int n = paintedtime - sent;
	int room = (int)(mx_snd_stream_room() / 4);
	if (n > room)
		n = room;
	while (n > 0) {
		int at = sent & (FRAMES - 1);
		int k = FRAMES - at < n ? FRAMES - at : n;
		int took = (int)(mx_snd_stream_queue(ring + at * 2, (u32)k * 4) / 4);
		sent += took;
		n -= took;
		if (took < k)
			break;
	}
}

void SNDDMA_Shutdown(void)
{
	mx_snd_stream_stop();
}
