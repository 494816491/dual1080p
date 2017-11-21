#include <iostream>
#include "rtmp_h264.h"
#include "compose.h"

int main(int argc, char *argv[])
{
    int s32Ret;

    rtmp_h264_server_start();
    s32Ret = SAMPLE_VIO_8_1080P_DUAL();

    return s32Ret;
}
