#ifndef STREAM_DISTRIBUTE_H
#define STREAM_DISTRIBUTE_H

#ifdef __cplusplus
extern "C"{
#endif

#include <hi_comm_venc.h>
#include <hi_comm_aenc.h>
#include <streamblock.h>
#include <pthread.h>

int stream_distri_init();

void translate_venc_stream(int channel,  VENC_STREAM_S *pstStream);
void translate_audio_stream(int channel, AUDIO_STREAM_S *pstStream);

int start_watch_routine();
#ifdef __cplusplus
}
#endif

#endif // STREAM_DISTRIBUTE_H
