/**
 * \file utils.c
 * \author WooShang <wooshang@126.com>
 * \date 2015/07/12
 *
 * This file provides some fuctions
 * Copyright (C) 2015 WooShang.
 *
 */
#include <stdlib.h>
#include <string.h>
#include <mpi_vb.h>
#include "utils.h"
#include "debug.h"
#include "common.h"
//#include "configure.h"
//#include "hi_mem.h"

int get_picture_size(PIC_SIZE_E enPicSize, SIZE_S *pstSize)
{
	switch(enPicSize) {
        case PIC_CIF:
            pstSize->u32Width = 352;
            pstSize->u32Height = 288;
            break;
        case PIC_D1:
            pstSize->u32Width = 704;
            pstSize->u32Height = 576;
            break;
        case PIC_HD720:
            pstSize->u32Width = 1280;
            pstSize->u32Height = 720;
			break;
        case PIC_HD1080:
            pstSize->u32Width = 1920;
            pstSize->u32Height = 1080;
			break;
		default:
			pstSize->u32Width = 704;
            pstSize->u32Height = 576;
			break;
	}
	return 0;
}

int get_param_from_mode(MAL_VI_MODE_E mode, MAL_VI_PARAM_S *pstViParam)
{
    switch (mode) {
        case MAL_VI_MODE_4_720P:
            pstViParam->s32ViDevCnt = 2;
            pstViParam->s32ViDevInterval = 1;
            pstViParam->s32ViChnCnt = 4;
            pstViParam->s32ViChnInterval = 2;
            break;
        
        case MAL_VI_MODE_4_1080P:
            pstViParam->s32ViDevCnt = 4;
            pstViParam->s32ViDevInterval = 2;
            pstViParam->s32ViChnCnt = 4;
            pstViParam->s32ViChnInterval = 4;            
            break;
        case MAL_VI_MODE_1_1080P:    
            pstViParam->s32ViDevCnt = 1;
            pstViParam->s32ViDevInterval = 1;
            pstViParam->s32ViChnCnt = 1;
            pstViParam->s32ViChnInterval = 1;
            break;

        /*For Hi3521*/
		case MAL_VI_MODE_8_D1:
            pstViParam->s32ViDevCnt = 2;
            pstViParam->s32ViDevInterval = 1;
            pstViParam->s32ViChnCnt = 8;
            pstViParam->s32ViChnInterval = 1;	
			break;
		case MAL_VI_MODE_1_720P:
            pstViParam->s32ViDevCnt = 1;
            pstViParam->s32ViDevInterval = 1;
            pstViParam->s32ViChnCnt = 1;
            pstViParam->s32ViChnInterval = 1;	
			break;

        default:
            err_msg("ViMode invaild!\n");
            return -1;
    }
    return 0;
}

int get_size_from_mode(MAL_VI_MODE_E mode, RECT_S *pstCapRect, SIZE_S *pstDestSize)
{
    pstCapRect->s32X = 0;
    pstCapRect->s32Y = 0;
    switch (mode) {
        case MAL_VI_MODE_1_D1:
        case MAL_VI_MODE_16_D1:
		case MAL_VI_MODE_8_D1:
            pstDestSize->u32Width = 704;
            pstDestSize->u32Height = 576;
            pstCapRect->u32Width = 704;
            pstCapRect->u32Height = 576;
            break;
        case MAL_VI_MODE_4_720P:
		case MAL_VI_MODE_1_720P:	
            pstDestSize->u32Width = 1280;
            pstDestSize->u32Height = 720;
            pstCapRect->u32Width = 1280;
            pstCapRect->u32Height = 720;
            break;
        case MAL_VI_MODE_4_1080P:
        case MAL_VI_MODE_1_1080P:
            pstDestSize->u32Width = 1920;
            pstDestSize->u32Height = 1080;
            pstCapRect->u32Width = 1920;
            pstCapRect->u32Height = 1080;
            break;

        default:
            err_msg("vi mode invaild!\n");
            return -1;
    }
    
    return 0;
}

int get_size_from_intfsync(VO_INTF_SYNC_E enIntfSync, SIZE_S *pstSize)
{
    switch (enIntfSync)
    {
        case VO_OUTPUT_PAL       :   pstSize->u32Width = 720;  pstSize->u32Height = 576; break;
        case VO_OUTPUT_NTSC      :   pstSize->u32Width = 720;  pstSize->u32Height = 480;  break;
        case VO_OUTPUT_800x600_60:   pstSize->u32Width = 800;  pstSize->u32Height = 600; break;
        case VO_OUTPUT_720P50    :   pstSize->u32Width = 1280; pstSize->u32Height = 720;   break;
        case VO_OUTPUT_1080P24  :   pstSize->u32Width = 1920; pstSize->u32Height = 1080;   break;
        case VO_OUTPUT_720P60    :  pstSize->u32Width = 1280; pstSize->u32Height = 720;   break;
        case VO_OUTPUT_1080P30   :   pstSize->u32Width = 1920; pstSize->u32Height = 1080;  break;
        case VO_OUTPUT_1080P25   :   pstSize->u32Width = 1920; pstSize->u32Height = 1080; break;
        case VO_OUTPUT_1080P50   :   pstSize->u32Width = 1920; pstSize->u32Height = 1080;  break;
        case VO_OUTPUT_1080P60   :   pstSize->u32Width = 1920; pstSize->u32Height = 1080;  break;
        case VO_OUTPUT_1024x768_60:   pstSize->u32Width = 1024; pstSize->u32Height = 768;   break;
        case VO_OUTPUT_1280x1024_60:   pstSize->u32Width = 1280; pstSize->u32Height = 1024;   break;
        case VO_OUTPUT_1366x768_60:    pstSize->u32Width = 1366; pstSize->u32Height = 768;   break;
        case VO_OUTPUT_1440x900_60:  pstSize->u32Width = 1440; pstSize->u32Height = 900;   break;
        case VO_OUTPUT_1280x800_60:  pstSize->u32Width = 1280; pstSize->u32Height = 800;   break;
        default: 
            err_msg("vo enIntfSync not support!\n");
            return -1;
    }
    return 0;
}
static void yuv_read_frame(FILE * fp, HI_U8 * pY, HI_U8 * pU, HI_U8 * pV, HI_U32 width, HI_U32 height, HI_U32 stride, HI_U32 stride2)
{   
    HI_U8 * pDst;
       
    HI_U32 u32Row;

    pDst = pY;
    for ( u32Row = 0; u32Row < height; u32Row++ )
    { 
        fread( pDst, width, 1, fp );
        pDst += stride;
    }
    
    pDst = pU;
    for ( u32Row = 0; u32Row < height/2; u32Row++ )
    {
        fread( pDst, width/2, 1, fp );
        pDst += stride2;
    }
    
    pDst = pV;
    for ( u32Row = 0; u32Row < height/2; u32Row++ )
    {
        fread( pDst, width/2, 1, fp );
        pDst += stride2;
    }
}

static void plan2remi(HI_U8 *pY, HI_S32 yStride, HI_U8 *pU, HI_S32 uStride,
               HI_U8 *pV, HI_S32 vStride, HI_S32 picWidth, HI_S32 picHeight)
{
    HI_S32 i;
    HI_U8* pTmpU, *ptu;
    HI_U8* pTmpV, *ptv;
    HI_S32 s32HafW = uStride >>1 ;
    HI_S32 s32HafH = picHeight >>1 ;
    HI_S32 s32Size = s32HafW*s32HafH;

    pTmpU = malloc( s32Size ); ptu = pTmpU;
    pTmpV = malloc( s32Size ); ptv = pTmpV;

    memcpy(pTmpU,pU,s32Size);
    memcpy(pTmpV,pV,s32Size);

    for(i = 0;i<s32Size>>1;i++) {
        *pU++ = *pTmpV++;
        *pU++ = *pTmpU++;

    }
    for(i = 0;i<s32Size>>1;i++) {
        *pV++ = *pTmpV++;
        *pV++ = *pTmpU++;
    }

    free( ptu );
    free( ptv );

}


int get_frame_from_yuv(const char* filename, int width, int height, int stride, VIDEO_FRAME_INFO_S *pstVFrameInfo)
{
	HI_U32     u32LStride,u32CStride, u32LumaSize;
    HI_U32     u32ChrmSize,u32PhyAddr;
    HI_U32     u32Size;
    VB_BLK VbBlk;
    HI_U8 *pVirAddr;
	FILE *fp;
	fp = fopen(filename, "rb");
	if(!fp) {
		err_msg("open file ->%s failed\n",filename);
		return -1;
	}

    u32LStride  = stride;
    u32CStride  = stride;

    u32LumaSize = (u32LStride * height);
    u32ChrmSize = (u32CStride * height) >> 2;/* YUV 420 */
    u32Size = u32LumaSize + (u32ChrmSize << 1);

    /* alloc video buffer block ---------------------------------------------------------- */
    VbBlk = HI_MPI_VB_GetBlock(VB_INVALID_POOLID, u32Size, NULL);
    if (VB_INVALID_HANDLE == VbBlk) {
        err_msg("HI_MPI_VB_GetBlock err! size:%d\n",u32Size);
		fclose(fp);
        return -1;
    }
    u32PhyAddr = HI_MPI_VB_Handle2PhysAddr(VbBlk);
    if (0 == u32PhyAddr) {
		fclose(fp);
        return -1;
    }

    pVirAddr = (HI_U8 *) HI_MPI_SYS_Mmap(u32PhyAddr, u32Size);
    if (NULL == pVirAddr) {
		fclose(fp);
        return -1;
    }

    pstVFrameInfo->u32PoolId = HI_MPI_VB_Handle2PoolId(VbBlk);
    if (VB_INVALID_POOLID == pstVFrameInfo->u32PoolId) {
		fclose(fp);
        return -1;
    }

	pstVFrameInfo->stVFrame.u32PhyAddr[0] = u32PhyAddr;
    pstVFrameInfo->stVFrame.u32PhyAddr[1] = pstVFrameInfo->stVFrame.u32PhyAddr[0] + u32LumaSize;
    pstVFrameInfo->stVFrame.u32PhyAddr[2] = pstVFrameInfo->stVFrame.u32PhyAddr[1] + u32ChrmSize;

    pstVFrameInfo->stVFrame.pVirAddr[0] = pVirAddr;
    pstVFrameInfo->stVFrame.pVirAddr[1] = pstVFrameInfo->stVFrame.pVirAddr[0] + u32LumaSize;
    pstVFrameInfo->stVFrame.pVirAddr[2] = pstVFrameInfo->stVFrame.pVirAddr[1] + u32ChrmSize;

    pstVFrameInfo->stVFrame.u32Width  = width;
    pstVFrameInfo->stVFrame.u32Height = height;
    pstVFrameInfo->stVFrame.u32Stride[0] = u32LStride;
    pstVFrameInfo->stVFrame.u32Stride[1] = u32CStride;
    pstVFrameInfo->stVFrame.u32Stride[2] = u32CStride;
    pstVFrameInfo->stVFrame.enPixelFormat = WIS_PIXEL_FORMAT;
    pstVFrameInfo->stVFrame.u32Field = VIDEO_FIELD_INTERLACED;/* Intelaced D1,otherwise VIDEO_FIELD_FRAME */

    /* read Y U V data from file to the addr ----------------------------------------------*/
    yuv_read_frame(fp, pstVFrameInfo->stVFrame.pVirAddr[0],
       pstVFrameInfo->stVFrame.pVirAddr[1], pstVFrameInfo->stVFrame.pVirAddr[2],
       pstVFrameInfo->stVFrame.u32Width, pstVFrameInfo->stVFrame.u32Height,
       pstVFrameInfo->stVFrame.u32Stride[0], pstVFrameInfo->stVFrame.u32Stride[1] >> 1 );

    /* convert planar YUV420 to sem-planar YUV420 -----------------------------------------*/
      plan2remi(pstVFrameInfo->stVFrame.pVirAddr[0], pstVFrameInfo->stVFrame.u32Stride[0],
      pstVFrameInfo->stVFrame.pVirAddr[1], pstVFrameInfo->stVFrame.u32Stride[1],
      pstVFrameInfo->stVFrame.pVirAddr[2], pstVFrameInfo->stVFrame.u32Stride[1],
      pstVFrameInfo->stVFrame.u32Width, pstVFrameInfo->stVFrame.u32Height);

    HI_MPI_SYS_Munmap(pVirAddr, u32Size);
	fclose(fp);
    return 0;
}

int mal_strcpy_s(char* strDestination, int numberOfElements, const char* strSource)
{
    char* p = strDestination;
    size_t available = numberOfElements;
    if (!strDestination || !strSource)
        return -1;
    if (strDestination == strSource)
        return 0;
    while ((*p++ = *strSource++) != 0 && --available > 0) {
        ;;
    }
    if (available == 0)
        strDestination[numberOfElements - 1] = 0;
    else
        *p = 0;
    return 0;
}
