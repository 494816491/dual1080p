#ifndef UDP_TS_H
#define UDP_TS_H
#include "streamblock.h"

#ifdef __cplusplus
extern "C"{
#endif

int rtmp_h264_server_start();
int start_rtmp_recv();
int send_rtmp_video_stream(Mal_StreamBlock *block);

#ifdef __cplusplus
}
#endif

#endif // UDP_TS_H
