#ifndef AUDIO_H
#define AUDIO_H
#ifdef __cplusplus
extern "C"{
#endif
int start_mpi_audio_stream();
int audio_hook_translate_audio_stream(void (*translate_audio_stream)(int channel,  VENC_STREAM_S *pstStream));
#ifdef __cplusplus
}
#endif
#endif // AUDIO_H
