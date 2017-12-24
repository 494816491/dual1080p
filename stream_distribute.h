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

int start_watch_routine();
#ifdef __cplusplus
}
#endif

#endif // STREAM_DISTRIBUTE_H
