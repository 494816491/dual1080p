#include <iostream>
#include <configdata.h>
#include <QString>
#include "rtmp_h264.h"
#include "compose.h"
#include "osd.h"
#include "container.h"
#include "audio.h"

int main(int argc, char *argv[])
{
    int s32Ret;

    ConfigData data("/mnt/rtmp_app.ini");
    //some init
    container_init(NULL);
    rtmp_h264_server_start();

    start_mpi_video_stream();
    start_mpi_audio_stream();

    osd_open();



    return s32Ret;
}
