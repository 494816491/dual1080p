#ifndef COMPOSE_H
#define COMPOSE_H
#ifdef __cplusplus
extern "C"{

#endif
#include "hi_type.h"
HI_S32 start_mpi_video_stream(HI_VOID);
int compose_hook_translate_venc_stream ();

int hisi_video_mem_init();
#ifdef __cplusplus
}
#endif
#endif // COMPOSE_H
