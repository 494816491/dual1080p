/**
 * \file utils.h
 * \author GoodMan <2757364047@qq.com>
 * \date 2015/12/14
 *
 * This file define all configure and ability that support by the board.
 * Copyright (C) 2015 GoodMan.
 *
 */
#ifndef COMMON_H
#define COMMON_H

#define MAX_AUDIO_SUPPORTS_CHANNELS	  8 /* 最大支持音频的通道数量 */

#define MAX_VI_SUPPORTS_CHANNELS      8  /* 最大支持的采集通道(路数) */

#define MAX_OSD_SUPPORT_AREARS		 1 /* 最大支持的OSD通道区域*/

#define MAX_VENC_SUPPORT_CHANNELS    16   /* 最大支持的编码通道数，主码流+子码流 */

#define MAKE_VENC_SUB_CHANNEL(x)   ((x)+8)  /* 主码流对应的子码流通道号 */

#define VIDEO_LOSS_FILE_PATH   		"/usr/share/loss.yuv"   /* 视频丢失信号的YUV文件 */

#define OSD_FONTS_FILE_PATH			"/usr/share/fonts/fontttf.ttf" /* OSD需要的字体文件 */
//#define OSD_FONTS_FILE_PATH			"/nfsroot/tmp/font/fontttf.ttf" /* OSD需要的字体文件 */

#define MAL_OSD_TEXT_FLAG_TIME   0x00000001
#define SAVE_FILE_TIME 60


typedef enum vi_mode_e
{   /* For Hi3531 or Hi3532 */
    STATUS_VI_MODE_PAL = 0,
    STATUS_VI_MODE_NTSC ,
    STATUS_VI_MODE_720P ,
}STATUS_VI_MODE_E;

#define VC_OSD_LENGTH 20
#define WIS_PIXEL_FORMAT PIXEL_FORMAT_YUV_SEMIPLANAR_420

#endif /* COMMON_H */
