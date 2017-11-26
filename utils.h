/**
 * \file utils.h
 * \author GoodMan <2757364047@qq.com>
 * \date 2015/07/12
 *
 * This file provides some fuctions
 * Copyright (C) 2015 GoodMan.
 *
 */
#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <mpi_sys.h>
#include <mpi_vo.h>

typedef enum mal_vi_mode_e
{   /* For Hi3531 or Hi3532 */
    MAL_VI_MODE_1_D1 = 0,
    MAL_VI_MODE_16_D1,
    MAL_VI_MODE_16_960H,
    MAL_VI_MODE_4_720P,
    MAL_VI_MODE_4_1080P,
    /* For Hi3521 */
	MAL_VI_MODE_8_D1,
	MAL_VI_MODE_1_720P,
	MAL_VI_MODE_16_Cif,
	MAL_VI_MODE_16_2Cif,
	MAL_VI_MODE_16_D1Cif,
	MAL_VI_MODE_1_D1Cif,
	/*For Hi3520A*/
    
    MAL_VI_MODE_4_D1,
    MAL_VI_MODE_8_2Cif,
    /*For Hi3520D*/
    MAL_VI_MODE_1_1080P,
    MAL_VI_MODE_8_D1Cif,
}MAL_VI_MODE_E;

typedef struct mal_vi_param_s
{
    int32_t s32ViDevCnt;		// VI Dev Total Count
    int32_t s32ViDevInterval;	// Vi Dev Interval
    int32_t s32ViChnCnt;		// Vi Chn Total Count
    int32_t s32ViChnInterval;	// VI Chn Interval
}MAL_VI_PARAM_S;

#ifdef __cplusplus
extern "C" {
#endif

int get_picture_size(PIC_SIZE_E enPicSize, SIZE_S *pstSize);

int get_param_from_mode(MAL_VI_MODE_E mode, MAL_VI_PARAM_S *pstViParam);

int get_size_from_mode(MAL_VI_MODE_E mode, RECT_S *pstCapRect, SIZE_S *pstDestSize);

int get_size_from_intfsync(VO_INTF_SYNC_E enIntfSync, SIZE_S *pstSize);

int get_frame_from_yuv(const char* filename, int width, int height, int stride, VIDEO_FRAME_INFO_S *pstVFrameInfo);

int mal_strcpy_s(char* strDestination, int numberOfElements, const char* strSource);

#ifdef __cplusplus
}
#endif
#endif /*UTILS_H*/
