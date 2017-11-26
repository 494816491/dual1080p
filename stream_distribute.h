#ifndef STREAM_DISTRIBUTE_H
#define STREAM_DISTRIBUTE_H

#include <hi_comm_venc.h>
#include <streamblock.h>

void translate_venc_stream(int channel,  VENC_STREAM_S *pstStream);
void translate_audio_stream(int channel, VENC_STREAM_S *pstStream);

#endif // STREAM_DISTRIBUTE_H
