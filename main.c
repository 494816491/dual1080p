#include "configdata.h"
#include "rtmp_h264.h"
#include "compose.h"
#include "osd.h"
#include "container.h"
#include "audio.h"
#include "stream_distribute.h"
#include "debug.h"

int main(int argc, char *argv[])
{
    int s32Ret;

    container_init(NULL);
    rtmp_h264_server_init();
    hisi_video_mem_init();

    stream_distri_init();
    start_mpi_video_stream();
#if 1
    start_mpi_audio_stream();
#endif
#if 1
    osd_open();
#endif
    info_msg("start over");
    start_watch_routine();

    return s32Ret;
}
