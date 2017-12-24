/******************************************************************************
  A simple program of Hisilicon HI3521A video input and output implementation.
  Copyright (C), 2014-2015, Hisilicon Tech. Co., Ltd.
 ******************************************************************************
    Modification:  2015-1 Created
******************************************************************************/

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* End of #ifdef __cplusplus */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <pthread.h>

#include <sys/sem.h>

#include "mpi_sys.h"
#include "mpi_vb.h"
#include "mpi_vi.h"
#include "mpi_vo.h"
#include "mpi_venc.h"
#include "mpi_vpss.h"
#include "mpi_vdec.h"
#include "mpi_vda.h"
#include "mpi_region.h"
#include "mpi_adec.h"
#include "mpi_aenc.h"
#include "mpi_ai.h"
#include "mpi_ao.h"
#include "mpi_hdmi.h"
#include "debug.h"

static struct compose_info_st{
    pthread_mutex_t mutex;
    void (*translate_venc_stream)(int channel,  VENC_STREAM_S *pstStream);
}compose_info;

static int compose_info_init()
{
    pthread_mutex_init(&compose_info.mutex, NULL);
    compose_info.translate_venc_stream = NULL;
    return 0;
}

int compose_hook_translate_venc_stream(void (*translate_venc_stream)(int channel,  VENC_STREAM_S *pstStream))
{
    pthread_mutex_lock(&compose_info.mutex);
    compose_info.translate_venc_stream = translate_venc_stream;
    pthread_mutex_unlock(&compose_info.mutex);
    return 0;
}


#define HDMI_SUPPORT
#define SAMPLE_SYS_ALIGN_WIDTH  16

typedef enum sample_rc_e
{
    SAMPLE_RC_CBR = 0,
    SAMPLE_RC_VBR,
    SAMPLE_RC_FIXQP
}SAMPLE_RC_E;

typedef enum sample_vi_mode_e
{
    SAMPLE_VI_MODE_16_D1,
    SAMPLE_VI_MODE_16_960H,
    SAMPLE_VI_MODE_16_1280H,
    SAMPLE_VI_MODE_16_HALF720P,

    SAMPLE_VI_MODE_8_720P,
    SAMPLE_VI_MODE_16_720P,

    SAMPLE_VI_MODE_4_1080P,
    SAMPLE_VI_MODE_8_1080P,
    SAMPLE_VI_MODE_16_1080P,

    SAMPLE_VI_MODE_4_3M,
}SAMPLE_VI_MODE_E;

typedef enum sample_vi_chn_set_e
{
    VI_CHN_SET_NORMAL = 0, /* mirror, filp close */
    VI_CHN_SET_MIRROR,      /* open MIRROR */
    VI_CHN_SET_FILP        /* open filp */
}SAMPLE_VI_CHN_SET_E;

//#define SAMPLE_PIXEL_FORMAT         PIXEL_FORMAT_YUV_SEMIPLANAR_420
#define SAMPLE_PIXEL_FORMAT         PIXEL_FORMAT_YUV_SEMIPLANAR_420

typedef struct sample_vi_param_s
{
    HI_S32 s32ViDevCnt;        // VI Dev Total Count
    HI_S32 s32ViDevInterval;   // Vi Dev Interval
    HI_S32 s32ViChnCnt;        // Vi Chn Total Count
    HI_S32 s32ViChnInterval;   // VI Chn Interval
}SAMPLE_VI_PARAM_S;


typedef enum sample_vo_mode_e
{
    VO_MODE_1MUX  = 0,
    VO_MODE_4MUX = 1,
    VO_MODE_9MUX = 2,
    VO_MODE_16MUX = 3,
    VO_MODE_BUTT
}SAMPLE_VO_MODE_E;


#define SAMPLE_VO_DEV_DHD0 0
#define SAMPLE_VO_DEV_DSD0 1
#define SAMPLE_VO_DEV_VIRT0 2
#define SAMPLE_VO_DEV_DSD1 -1

#define SAMPLE_VO_LAYER_VHD0 0
#define SAMPLE_VO_LAYER_VSD0 2
#define SAMPLE_VO_LAYER_VIRT0 3
#define SAMPLE_VO_LAYER_VPIP 1

#define ALIGN_BACK(x, a)              ((a) * (((x) / (a))))

typedef struct hiHDMI_CALLBACK_ARGS_S
{
    HI_HDMI_ID_E		enHdmi;
    HI_HDMI_VIDEO_FMT_E eForceFmt;
}HDMI_CALLBACK_ARGS_S;

#define SAMPLE_PRT(fmt...)   \
    do {\
        printf("[%s]-%d: ", __FUNCTION__, __LINE__);\
        printf(fmt);\
       }while(0)

#define D1_WIDTH                720
#define D1_HEIGHT               576

#define _720P_WIDTH              1280
#define _720P_HEIGHT             720

#define HD_WIDTH                1920
#define HD_HEIGHT               1080

/******************************************************************************
* function : get picture size(w*h), according Norm and enPicSize
******************************************************************************/
HI_S32 SAMPLE_COMM_SYS_GetPicSize(VIDEO_NORM_E enNorm, PIC_SIZE_E enPicSize, SIZE_S *pstSize)
{
    switch (enPicSize)
    {
        case PIC_QCIF:
            pstSize->u32Width = D1_WIDTH / 4;
            pstSize->u32Height = (VIDEO_ENCODING_MODE_PAL==enNorm)?144:120;
            break;
        case PIC_CIF:
            pstSize->u32Width = D1_WIDTH / 2;
            pstSize->u32Height = (VIDEO_ENCODING_MODE_PAL==enNorm)?288:240;
            break;
        case PIC_D1:
            pstSize->u32Width = D1_WIDTH;
            pstSize->u32Height = (VIDEO_ENCODING_MODE_PAL==enNorm)?576:480;
            break;
        case PIC_960H:
            pstSize->u32Width = 960;
            pstSize->u32Height = (VIDEO_ENCODING_MODE_PAL==enNorm)?576:480;
            break;
        case PIC_2CIF:
            pstSize->u32Width = D1_WIDTH / 2;
            pstSize->u32Height = (VIDEO_ENCODING_MODE_PAL==enNorm)?576:480;
            break;
        case PIC_QVGA:    /* 320 * 240 */
            pstSize->u32Width = 320;
            pstSize->u32Height = 240;
            break;
        case PIC_VGA:     /* 640 * 480 */
            pstSize->u32Width = 640;
            pstSize->u32Height = 480;
            break;
        case PIC_XGA:     /* 1024 * 768 */
            pstSize->u32Width = 1024;
            pstSize->u32Height = 768;
            break;
        case PIC_SXGA:    /* 1400 * 1050 */
            pstSize->u32Width = 1400;
            pstSize->u32Height = 1050;
            break;
        case PIC_UXGA:    /* 1600 * 1200 */
            pstSize->u32Width = 1600;
            pstSize->u32Height = 1200;
            break;
        case PIC_QXGA:    /* 2048 * 1536 */
            pstSize->u32Width = 2048;
            pstSize->u32Height = 1536;
            break;
        case PIC_WVGA:    /* 854 * 480 */
            pstSize->u32Width = 854;
            pstSize->u32Height = 480;
            break;
        case PIC_WSXGA:   /* 1680 * 1050 */
            pstSize->u32Width = 1680;
            pstSize->u32Height = 1050;
            break;
        case PIC_WUXGA:   /* 1920 * 1200 */
            pstSize->u32Width = 1920;
            pstSize->u32Height = 1200;
            break;
        case PIC_WQXGA:   /* 2560 * 1600 */
            pstSize->u32Width = 2560;
            pstSize->u32Height = 1600;
            break;
        case PIC_HD720:   /* 1280 * 720 */
            pstSize->u32Width = 1280;
            pstSize->u32Height = 720;
            break;
        case PIC_HD1080:  /* 1920 * 1080 */
            pstSize->u32Width = 1920;
            pstSize->u32Height = 1080;
            break;
        default:
            return HI_FAILURE;
    }
    return HI_SUCCESS;
}



/******************************************************************************
* function : calculate VB Block size of picture.
******************************************************************************/
HI_U32 SAMPLE_COMM_SYS_CalcPicVbBlkSize(VIDEO_NORM_E enNorm, PIC_SIZE_E enPicSize, PIXEL_FORMAT_E enPixFmt, HI_U32 u32AlignWidth,COMPRESS_MODE_E enCompFmt)
{
    HI_S32 s32Ret      		= HI_FAILURE;
    SIZE_S stSize;
    HI_U32 u32Width 		= 0;
    HI_U32 u32Height 		= 0;
    HI_U32 u32BlkSize 		= 0;
    HI_U32 u32HeaderSize 	= 0;

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enNorm, enPicSize, &stSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("get picture size[%d] failed!\n", enPicSize);
            return HI_FAILURE;
    }

    if (PIXEL_FORMAT_YUV_SEMIPLANAR_422 != enPixFmt && PIXEL_FORMAT_YUV_SEMIPLANAR_420 != enPixFmt)
    {
        SAMPLE_PRT("pixel format[%d] input failed!\n", enPixFmt);
            return HI_FAILURE;
    }

    if (16!=u32AlignWidth && 32!=u32AlignWidth && 64!=u32AlignWidth)
    {
        SAMPLE_PRT("system align width[%d] input failed!\n",\
               u32AlignWidth);
            return HI_FAILURE;
    }
    if (704 == stSize.u32Width)
    {
        stSize.u32Width = 720;
    }
    //SAMPLE_PRT("w:%d, u32AlignWidth:%d\n", CEILING_2_POWER(stSize.u32Width,u32AlignWidth), u32AlignWidth);

    u32Width  = CEILING_2_POWER(stSize.u32Width, u32AlignWidth);
    u32Height = CEILING_2_POWER(stSize.u32Height,u32AlignWidth);

    if (PIXEL_FORMAT_YUV_SEMIPLANAR_422 == enPixFmt)
    {
        u32BlkSize = u32Width * u32Height * 2;
    }
    else
    {
        u32BlkSize = u32Width * u32Height * 3 / 2;
    }


    if(COMPRESS_MODE_SEG == enCompFmt)
    {
        VB_PIC_HEADER_SIZE(u32Width,u32Height,enPixFmt,u32HeaderSize);
    }

    u32BlkSize += u32HeaderSize;

    return u32BlkSize;
}

/******************************************************************************
* function : vb init & MPI system init
******************************************************************************/
HI_S32 SAMPLE_COMM_SYS_Init(VB_CONF_S *pstVbConf)
{
    MPP_SYS_CONF_S stSysConf = {0};
    HI_S32 s32Ret = HI_FAILURE;
    HI_S32 i;

    HI_MPI_SYS_Exit();

    for(i=0;i<VB_MAX_USER;i++)
    {
         HI_MPI_VB_ExitModCommPool(i);
    }
    for(i=0; i<VB_MAX_POOLS; i++)
    {
         HI_MPI_VB_DestroyPool(i);
    }
    HI_MPI_VB_Exit();

    if (NULL == pstVbConf)
    {
        SAMPLE_PRT("input parameter is null, it is invaild!\n");
        return HI_FAILURE;
    }

    s32Ret = HI_MPI_VB_SetConf(pstVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VB_SetConf failed!\n");
        return HI_FAILURE;
    }

    s32Ret = HI_MPI_VB_Init();
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VB_Init failed!\n");
        return HI_FAILURE;
    }

    stSysConf.u32AlignWidth = SAMPLE_SYS_ALIGN_WIDTH;
    s32Ret = HI_MPI_SYS_SetConf(&stSysConf);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_SYS_SetConf failed\n");
        return HI_FAILURE;
    }

    s32Ret = HI_MPI_SYS_Init();
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_SYS_Init failed!\n");
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

/*****************************************************************************
* function : star vi chn
*****************************************************************************/
HI_S32 SAMPLE_COMM_VI_StartChn(VI_CHN ViChn, RECT_S *pstCapRect, SIZE_S *pstTarSize,
    SAMPLE_VI_MODE_E enViMode, SAMPLE_VI_CHN_SET_E enViChnSet)
{
    HI_S32 s32Ret;
    VI_CHN_ATTR_S stChnAttr;

    /* step  5: config & start vicap dev */
    memcpy(&stChnAttr.stCapRect, pstCapRect, sizeof(RECT_S));
    /* to show scale. this is a sample only, we want to show dist_size = D1 only */
    stChnAttr.stDestSize.u32Width = pstTarSize->u32Width;
    stChnAttr.stDestSize.u32Height = pstTarSize->u32Height;
    stChnAttr.enCapSel = VI_CAPSEL_BOTH;
    stChnAttr.enPixFormat = SAMPLE_PIXEL_FORMAT;   /* sp420 or sp422 */
    stChnAttr.bMirror = (VI_CHN_SET_MIRROR == enViChnSet)?HI_TRUE:HI_FALSE;
    stChnAttr.bFlip = (VI_CHN_SET_FILP == enViChnSet)?HI_TRUE:HI_FALSE;
    stChnAttr.s32SrcFrameRate = -1;
    stChnAttr.s32DstFrameRate = -1;
    switch (enViMode)
    {
        case SAMPLE_VI_MODE_16_D1:
        case SAMPLE_VI_MODE_16_960H:
        case SAMPLE_VI_MODE_16_1280H:
            stChnAttr.enScanMode = VI_SCAN_INTERLACED;
            break;
        case SAMPLE_VI_MODE_16_HALF720P:
        case SAMPLE_VI_MODE_8_720P:
        case SAMPLE_VI_MODE_16_720P:
        case SAMPLE_VI_MODE_4_1080P:
        case SAMPLE_VI_MODE_8_1080P:
        case SAMPLE_VI_MODE_16_1080P:
        case SAMPLE_VI_MODE_4_3M:
            stChnAttr.enScanMode = VI_SCAN_PROGRESSIVE;
            //stChnAttr.enScanMode = VI_SCAN_INTERLACED;
            break;
        default:
            SAMPLE_PRT("ViMode invaild!\n");
            return HI_FAILURE;
    }

    s32Ret = HI_MPI_VI_SetChnAttr(ViChn, &stChnAttr);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    s32Ret = HI_MPI_VI_EnableChn(ViChn);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

HI_S32 SAMPLE_COMM_VI_Mode2Param(SAMPLE_VI_MODE_E enViMode, SAMPLE_VI_PARAM_S *pstViParam)
{
    switch (enViMode)
    {
        case SAMPLE_VI_MODE_16_D1:
        case SAMPLE_VI_MODE_16_960H:
        case SAMPLE_VI_MODE_16_1280H:
        case SAMPLE_VI_MODE_16_HALF720P:
            pstViParam->s32ViDevCnt = 4;
            pstViParam->s32ViDevInterval = 1;
            pstViParam->s32ViChnCnt = 16;
            pstViParam->s32ViChnInterval = 1;
            break;
        case SAMPLE_VI_MODE_8_720P:
        case SAMPLE_VI_MODE_8_1080P:
            /* use chn 0,2,4,6,8,10,12,14 */
            pstViParam->s32ViDevCnt = 2;
            pstViParam->s32ViDevInterval = 1;
            pstViParam->s32ViChnCnt = 2;
            //pstViParam->s32ViChnInterval = 4;
            pstViParam->s32ViChnInterval = 2;
            break;
        case SAMPLE_VI_MODE_4_1080P:
        case SAMPLE_VI_MODE_4_3M:
            /* use chn 0,4,8,12 */
            pstViParam->s32ViDevCnt = 4;
            pstViParam->s32ViDevInterval = 1;
            pstViParam->s32ViChnCnt = 4;
            pstViParam->s32ViChnInterval = 4;
            break;
        default:
            SAMPLE_PRT("ViMode invaild!\n");
            return HI_FAILURE;
    }
    return HI_SUCCESS;
}


/*****************************************************************************
* function : get vi parameter, according to vi type
*****************************************************************************/
HI_S32 SAMPLE_COMM_VI_Mode2Size(SAMPLE_VI_MODE_E enViMode, VIDEO_NORM_E enNorm, RECT_S *pstCapRect, SIZE_S *pstDestSize)
{
    pstCapRect->s32X = 0;
    pstCapRect->s32Y = 0;
    switch (enViMode)
    {
        case SAMPLE_VI_MODE_16_D1:
            pstDestSize->u32Width = D1_WIDTH;
            pstDestSize->u32Height = (VIDEO_ENCODING_MODE_PAL==enNorm)?576:480;
            pstCapRect->u32Width = D1_WIDTH;
            pstCapRect->u32Height = (VIDEO_ENCODING_MODE_PAL==enNorm)?576:480;
            break;
        case SAMPLE_VI_MODE_16_960H:
            pstDestSize->u32Width = 960;
            pstDestSize->u32Height = (VIDEO_ENCODING_MODE_PAL==enNorm)?576:480;
            pstCapRect->u32Width = 960;
            pstCapRect->u32Height = (VIDEO_ENCODING_MODE_PAL==enNorm)?576:480;
            break;
        case SAMPLE_VI_MODE_8_720P:
        case SAMPLE_VI_MODE_16_720P:
            pstDestSize->u32Width  = _720P_WIDTH;
            pstDestSize->u32Height = _720P_HEIGHT;
            pstCapRect->u32Width  = _720P_WIDTH;
            pstCapRect->u32Height = _720P_HEIGHT;
            break;

     case SAMPLE_VI_MODE_8_1080P:
            pstDestSize->u32Width  = HD_WIDTH;
            pstDestSize->u32Height = HD_HEIGHT;
            pstCapRect->u32Width  = HD_WIDTH;
            pstCapRect->u32Height = HD_HEIGHT;
            break;
     case SAMPLE_VI_MODE_4_1080P:
            pstDestSize->u32Width  = HD_WIDTH;
            pstDestSize->u32Height = HD_HEIGHT;
            pstCapRect->u32Width  = HD_WIDTH;
            pstCapRect->u32Height = HD_HEIGHT;
            break;

        default:
            SAMPLE_PRT("vi mode invaild!\n");
            return HI_FAILURE;
    }

    return HI_SUCCESS;
}

VI_DEV_ATTR_S DEV_ATTR_BT656D1_4MUX =
{
    /*interface mode*/
    VI_MODE_BT656,
    /*work mode*/
    VI_WORK_MODE_4Multiplex,
    /* r_mask    g_mask    b_mask*/
    {0xFF000000,    0x0},

    /* for single/double edge, must be set when double edge*/
    VI_CLK_EDGE_SINGLE_UP,

    /*AdChnId*/
    {-1, -1, -1, -1},
    /*enDataSeq, just support YUV*/
    VI_INPUT_DATA_YVYU,
    /**/
    {
    /*port_vsync   port_vsync_neg     port_hsync        port_hsync_neg        */
    VI_VSYNC_FIELD, VI_VSYNC_NEG_HIGH, VI_HSYNC_VALID_SINGNAL,VI_HSYNC_NEG_HIGH,VI_VSYNC_VALID_SINGAL,VI_VSYNC_VALID_NEG_HIGH,

    /**/
    /*hsync_hfb    hsync_act    hsync_hhb*/
    {0,            0,        0,
    /*vsync0_vhb vsync0_act vsync0_hhb*/
     0,            0,        0,
    /*vsync1_vhb vsync1_act vsync1_hhb*/
     0,            0,            0}
    },
    /*whether use isp*/
    VI_PATH_BYPASS,
    /*data type*/
    VI_DATA_TYPE_YUV
};

VI_DEV_ATTR_S DEV_ATTR_6114_720P_2MUX_BASE =
{
    /*interface mode*/
    VI_MODE_BT656,
    /*work mode*/
    VI_WORK_MODE_2Multiplex,
    /* r_mask    g_mask    b_mask*/
    {0xFF000000,    0x0},
    //{0x0,    0x0},

    /* for single/double edge, must be set when double edge*/
    VI_CLK_EDGE_SINGLE_UP,

    /*AdChnId*/
    {-1, -1, -1, -1},
    /*enDataSeq, just support YUV*/
    VI_INPUT_DATA_YVYU,
    /**/
    {
    /*port_vsync   port_vsync_neg     port_hsync        port_hsync_neg        */
    VI_VSYNC_FIELD, VI_VSYNC_NEG_HIGH, VI_HSYNC_VALID_SINGNAL,VI_HSYNC_NEG_HIGH,VI_VSYNC_VALID_SINGAL,VI_VSYNC_VALID_NEG_HIGH,

    /**/
    /*hsync_hfb    hsync_act    hsync_hhb*/
    {0,            0,        0,
    /*vsync0_vhb vsync0_act vsync0_hhb*/
     0,            0,        0,
    /*vsync1_vhb vsync1_act vsync1_hhb*/
     0,            0,            0}
    },
    /*whether use isp*/
    VI_PATH_BYPASS,
    /*data type*/
    VI_DATA_TYPE_YUV
};

VI_DEV_ATTR_S DEV_ATTR_6124b_21080P_2MUX_BASE =
{
    /*interface mode*/
    VI_MODE_BT656,
    /*work mode*/
    VI_WORK_MODE_2Multiplex,
    /* r_mask    g_mask    b_mask*/
    //{0xFF000000,    0x0},
    {0xFF000000,    0xff000000},
    //{0x0,    0x0},

    /* for single/double edge, must be set when double edge*/
    VI_CLK_EDGE_DOUBLE,

    /*AdChnId*/
    {-1, -1, -1, -1},
    /*enDataSeq, just support YUV*/
    VI_INPUT_DATA_YVYU,
    /**/
    {
    /*port_vsync   port_vsync_neg     port_hsync        port_hsync_neg        */
    VI_VSYNC_FIELD, VI_VSYNC_NEG_HIGH, VI_HSYNC_VALID_SINGNAL,VI_HSYNC_NEG_HIGH,VI_VSYNC_VALID_SINGAL,VI_VSYNC_VALID_NEG_HIGH,

    /**/
    /*hsync_hfb    hsync_act    hsync_hhb*/
    {0,            0,        0,
    /*vsync0_vhb vsync0_act vsync0_hhb*/
     0,            0,        0,
    /*vsync1_vhb vsync1_act vsync1_hhb*/
     0,            0,            0}
    },
    /*whether use isp*/
    VI_PATH_BYPASS,
    /*data type*/
    VI_DATA_TYPE_YUV
};

/*****************************************************************************
* function : set vi mask.
*****************************************************************************/
HI_VOID SAMPLE_COMM_VI_SetMask(VI_DEV ViDev, VI_DEV_ATTR_S *pstDevAttr)
{
    switch (ViDev % 4)
    {
        case 0:
            pstDevAttr->au32CompMask[0] = 0xFF;
            if (VI_MODE_BT1120_STANDARD == pstDevAttr->enIntfMode)
            {
                pstDevAttr->au32CompMask[1] = 0x00FF0000;
            }
            else if (VI_MODE_BT1120_INTERLEAVED == pstDevAttr->enIntfMode)
            {
                pstDevAttr->au32CompMask[1] = 0x0;
            }
            break;
        case 1:
            pstDevAttr->au32CompMask[0] = 0xFF00;
            if (VI_MODE_BT1120_INTERLEAVED == pstDevAttr->enIntfMode)
            {
                pstDevAttr->au32CompMask[1] = 0x0;
            }
            break;
        case 2:
            pstDevAttr->au32CompMask[0] = 0xFF0000;
            if (VI_MODE_BT1120_STANDARD == pstDevAttr->enIntfMode)
            {
                pstDevAttr->au32CompMask[1] = 0xFF;
            }
            else if (VI_MODE_BT1120_INTERLEAVED == pstDevAttr->enIntfMode)
            {
                pstDevAttr->au32CompMask[1] = 0x0;
            }
            break;
        case 3:
            pstDevAttr->au32CompMask[0] = 0xFF000000;
            if (VI_MODE_BT1120_INTERLEAVED == pstDevAttr->enIntfMode)
            {
                pstDevAttr->au32CompMask[1] = 0x0;
            }
            break;
        default:
            HI_ASSERT(0);
    }
}

/*****************************************************************************
* function : star vi dev (cfg vi_dev_attr; set_dev_cfg; enable dev)
*****************************************************************************/
HI_S32 SAMPLE_COMM_VI_StartDev(VI_DEV ViDev, SAMPLE_VI_MODE_E enViMode)
{
    HI_S32 s32Ret;
    VI_DEV_ATTR_S stViDevAttr;
    memset(&stViDevAttr,0,sizeof(stViDevAttr));

    switch (enViMode)
    {
        case SAMPLE_VI_MODE_16_D1:
            memcpy(&stViDevAttr,&DEV_ATTR_BT656D1_4MUX,sizeof(stViDevAttr));
            SAMPLE_COMM_VI_SetMask(ViDev,&stViDevAttr);
            break;
        case SAMPLE_VI_MODE_16_960H:
            memcpy(&stViDevAttr,&DEV_ATTR_BT656D1_4MUX,sizeof(stViDevAttr));
            SAMPLE_COMM_VI_SetMask(ViDev,&stViDevAttr);
            break;
        case SAMPLE_VI_MODE_8_720P:
        case SAMPLE_VI_MODE_16_720P:
            memcpy(&stViDevAttr,&DEV_ATTR_6114_720P_2MUX_BASE,sizeof(stViDevAttr));
            SAMPLE_COMM_VI_SetMask(ViDev,&stViDevAttr);
            break;
        case SAMPLE_VI_MODE_8_1080P:
            memcpy(&stViDevAttr,&DEV_ATTR_6124b_21080P_2MUX_BASE,sizeof(stViDevAttr));
            SAMPLE_COMM_VI_SetMask(ViDev,&stViDevAttr);
        break;
        default:
            SAMPLE_PRT("vi input type[%d] is invalid!\n", enViMode);
            return HI_FAILURE;
    }
#if 0
    if(SAMPLE_VI_MODE_8_1080P == enViMode)
    {
        stViDevAttr.enClkEdge = VI_CLK_EDGE_DOUBLE;
    }
#endif
    s32Ret = HI_MPI_VI_SetDevAttr(ViDev, &stViDevAttr);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_VI_SetDevAttr failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    s32Ret = HI_MPI_VI_EnableDev(ViDev);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_VI_EnableDev failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    return HI_SUCCESS;
}

/*****************************************************************************
* function : star vi according to product type
*            if vi input is hd, we will start sub-chn for cvbs preview
*****************************************************************************/
HI_S32 SAMPLE_COMM_VI_Start(SAMPLE_VI_MODE_E enViMode, VIDEO_NORM_E enNorm)
{
    VI_DEV ViDev;
    VI_CHN ViChn;
    HI_S32 i;
    HI_S32 s32Ret;
    SAMPLE_VI_PARAM_S stViParam;
    SIZE_S stTargetSize;
    RECT_S stCapRect;
    VI_CHN_BIND_ATTR_S stChnBindAttr;

    /*** get parameter from Sample_Vi_Mode ***/
    s32Ret = SAMPLE_COMM_VI_Mode2Param(enViMode, &stViParam);
    if (HI_SUCCESS !=s32Ret)
    {
        SAMPLE_PRT("vi get param failed!\n");
        return HI_FAILURE;
    }
    s32Ret = SAMPLE_COMM_VI_Mode2Size(enViMode, enNorm, &stCapRect, &stTargetSize);
    if (HI_SUCCESS !=s32Ret)
    {
        SAMPLE_PRT("vi get size failed!\n");
        return HI_FAILURE;
    }

    /*** Start AD ***/
#if 0
    s32Ret = SAMPLE_COMM_VI_ADStart(enViMode, enNorm);
    if (HI_SUCCESS !=s32Ret)
    {
        SAMPLE_PRT("Start AD failed!\n");
        return HI_FAILURE;
    }
#endif

    /*** Start VI Dev ***/
    for(i=0; i<stViParam.s32ViDevCnt; i++)
    {
        ViDev = i * stViParam.s32ViDevInterval;
        s32Ret = SAMPLE_COMM_VI_StartDev(ViDev, enViMode);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("SAMPLE_COMM_VI_StartDev failed with %#x\n", s32Ret);
            return HI_FAILURE;
        }
        //HI_MPI_VI_DisableDllSlave(ViDev);
    }

    /*** Start VI Chn ***/
    for(i=0; i<stViParam.s32ViChnCnt; i++)
    {
        ViChn = i * stViParam.s32ViChnInterval;

        if (SAMPLE_VI_MODE_8_1080P == enViMode
         || SAMPLE_VI_MODE_8_720P == enViMode)
        {
            /* When in the 8x1080p mode, bind chn 2,6,10,14 to way1 is needed */
#if 1
            if (ViChn%4 != 0)
            {
                s32Ret = HI_MPI_VI_GetChnBind(ViChn, &stChnBindAttr);
                if (HI_ERR_VI_FAILED_NOTBIND == s32Ret)
                {
                    stChnBindAttr.ViDev = ViChn/4;
                    stChnBindAttr.ViWay = 1;
                    s32Ret = HI_MPI_VI_BindChn(ViChn, &stChnBindAttr);
                    if (HI_SUCCESS != s32Ret)
                    {
                        SAMPLE_PRT("call HI_MPI_VI_BindChn failed with %#x\n", s32Ret);
                        return HI_FAILURE;
                    }
                }
            }
#endif
        }

        s32Ret = SAMPLE_COMM_VI_StartChn(ViChn, &stCapRect, &stTargetSize, enViMode, VI_CHN_SET_NORMAL);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("call SAMPLE_COMM_VI_StarChn failed with %#x\n", s32Ret);
            return HI_FAILURE;
        }
    }

    return HI_SUCCESS;
}

/*****************************************************************************
* function : start vpss. VPSS chn with frame
*****************************************************************************/
HI_S32 SAMPLE_COMM_VPSS_Start(HI_S32 s32GrpCnt, SIZE_S *pstSize, HI_S32 s32ChnCnt,VPSS_GRP_ATTR_S *pstVpssGrpAttr)
{
    VPSS_GRP VpssGrp;
    VPSS_CHN VpssChn;
    VPSS_GRP_ATTR_S stGrpAttr = {0};
    VPSS_CHN_ATTR_S stChnAttr = {0};
    VPSS_GRP_PARAM_S stVpssParam = {0};
    HI_S32 s32Ret;
    HI_S32 i, j;
    VPSS_CHN_MODE_S stVpssMode = {0};

    /*** Set Vpss Grp Attr ***/

    if(NULL == pstVpssGrpAttr)
    {
        stGrpAttr.u32MaxW = pstSize->u32Width;
        stGrpAttr.u32MaxH = pstSize->u32Height;
        stGrpAttr.enPixFmt = SAMPLE_PIXEL_FORMAT;

        stGrpAttr.bIeEn = HI_FALSE;
        stGrpAttr.bNrEn = HI_TRUE;
        stGrpAttr.bDciEn = HI_FALSE;
        stGrpAttr.bHistEn = HI_FALSE;
        stGrpAttr.bEsEn = HI_FALSE;
        stGrpAttr.enDieMode = VPSS_DIE_MODE_NODIE;
    }
    else
    {
        memcpy(&stGrpAttr,pstVpssGrpAttr,sizeof(VPSS_GRP_ATTR_S));
    }


    for(i=0; i<s32GrpCnt; i++)
    {
        VpssGrp = i;
        /*** create vpss group ***/
        s32Ret = HI_MPI_VPSS_CreateGrp(VpssGrp, &stGrpAttr);
        if (s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("HI_MPI_VPSS_CreateGrp failed with %#x!\n", s32Ret);
            return HI_FAILURE;
        }

        /*** set vpss param ***/
        s32Ret = HI_MPI_VPSS_GetGrpParam(VpssGrp, &stVpssParam);
        if (s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("failed with %#x!\n", s32Ret);
            return HI_FAILURE;
        }

        stVpssParam.u32IeStrength = 0;
        s32Ret = HI_MPI_VPSS_SetGrpParam(VpssGrp, &stVpssParam);
        if (s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("failed with %#x!\n", s32Ret);
            return HI_FAILURE;
        }

        /*** enable vpss chn, with frame ***/
        for(j=0; j<s32ChnCnt; j++)
        {
            VpssChn = j;
            /* Set Vpss Chn attr */
            stChnAttr.bSpEn = HI_FALSE;
            stChnAttr.bUVInvert = HI_FALSE;
            stChnAttr.bBorderEn = HI_TRUE;
            stChnAttr.stBorder.u32Color = 0xff00;
            stChnAttr.stBorder.u32LeftWidth = 2;
            stChnAttr.stBorder.u32RightWidth = 2;
            stChnAttr.stBorder.u32TopWidth = 2;
            stChnAttr.stBorder.u32BottomWidth = 2;

            s32Ret = HI_MPI_VPSS_SetChnAttr(VpssGrp, VpssChn, &stChnAttr);
            if (s32Ret != HI_SUCCESS)
            {
                SAMPLE_PRT("HI_MPI_VPSS_SetChnAttr failed with %#x\n", s32Ret);
                return HI_FAILURE;
            }

            stVpssMode.enChnMode = VPSS_CHN_MODE_USER;
#if 1
#if 0
            stVpssMode.u32Width  = 720/3;
            stVpssMode.u32Height = 480/3;
#else
            stVpssMode.u32Width  = 1920;
            stVpssMode.u32Height = 1080;
#endif
#else
            stVpssMode.u32Width  = 1980;
            stVpssMode.u32Height = 1080;
#endif
            stVpssMode.enPixelFormat = PIXEL_FORMAT_YUV_SEMIPLANAR_420;
            s32Ret = HI_MPI_VPSS_SetChnMode(VpssGrp, VPSS_CHN3, &stVpssMode);
            if (s32Ret != HI_SUCCESS)
            {
                SAMPLE_PRT("HI_MPI_VPSS_SetChnMode failed with %#x\n", s32Ret);
                return HI_FAILURE;
            }

            s32Ret = HI_MPI_VPSS_EnableChn(VpssGrp, VpssChn);
            if (s32Ret != HI_SUCCESS)
            {
                SAMPLE_PRT("HI_MPI_VPSS_EnableChn failed with %#x\n", s32Ret);
                return HI_FAILURE;
            }
        }

        /*** start vpss group ***/
        s32Ret = HI_MPI_VPSS_StartGrp(VpssGrp);
        if (s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("HI_MPI_VPSS_StartGrp failed with %#x\n", s32Ret);
            return HI_FAILURE;
        }

    }
    return HI_SUCCESS;
}

/*****************************************************************************
* function : Vi chn bind vpss group
*****************************************************************************/
HI_S32 SAMPLE_COMM_VI_BindVpss(SAMPLE_VI_MODE_E enViMode)
{
    HI_S32 j, s32Ret;
    VPSS_GRP VpssGrp;
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;
    SAMPLE_VI_PARAM_S stViParam;
    VI_CHN ViChn;

    s32Ret = SAMPLE_COMM_VI_Mode2Param(enViMode, &stViParam);
    if (HI_SUCCESS !=s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_Mode2Param failed!\n");
        return HI_FAILURE;
    }

    VpssGrp = 0;
    for (j=0; j<stViParam.s32ViChnCnt; j++)
    {
        ViChn = j * stViParam.s32ViChnInterval;

        stSrcChn.enModId = HI_ID_VIU;
        stSrcChn.s32DevId = 0;
        stSrcChn.s32ChnId = ViChn;

        stDestChn.enModId = HI_ID_VPSS;
        stDestChn.s32DevId = VpssGrp;
        stDestChn.s32ChnId = 0;

        s32Ret = HI_MPI_SYS_Bind(&stSrcChn, &stDestChn);
        if (s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("failed with %#x!\n", s32Ret);
            return HI_FAILURE;
        }

        VpssGrp ++;
    }
    return HI_SUCCESS;
}

HI_S32 SAMPLE_COMM_VO_StartDev(VO_DEV VoDev, VO_PUB_ATTR_S *pstPubAttr)
{
    HI_S32 s32Ret = HI_SUCCESS;

    s32Ret = HI_MPI_VO_SetPubAttr(VoDev, pstPubAttr);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    s32Ret = HI_MPI_VO_Enable(VoDev);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    return s32Ret;
}

HI_S32 SAMPLE_COMM_VO_GetWH(VO_INTF_SYNC_E enIntfSync, HI_U32 *pu32W,HI_U32 *pu32H, HI_U32 *pu32Frm)
{
    switch (enIntfSync)
    {
        case VO_OUTPUT_PAL       :  *pu32W = 720;  *pu32H = 576;  *pu32Frm = 25; break;
        case VO_OUTPUT_NTSC      :  *pu32W = 720;  *pu32H = 480;  *pu32Frm = 30; break;
        case VO_OUTPUT_576P50    :  *pu32W = 720;  *pu32H = 576;  *pu32Frm = 50; break;
        case VO_OUTPUT_480P60    :  *pu32W = 720;  *pu32H = 480;  *pu32Frm = 60; break;
        case VO_OUTPUT_800x600_60:  *pu32W = 800;  *pu32H = 600;  *pu32Frm = 60; break;
        case VO_OUTPUT_720P50    :  *pu32W = 1280; *pu32H = 720;  *pu32Frm = 50; break;
        case VO_OUTPUT_720P60    :  *pu32W = 1280; *pu32H = 720;  *pu32Frm = 60; break;
        case VO_OUTPUT_1080I50   :  *pu32W = 1920; *pu32H = 1080; *pu32Frm = 50; break;
        case VO_OUTPUT_1080I60   :  *pu32W = 1920; *pu32H = 1080; *pu32Frm = 60; break;
        case VO_OUTPUT_1080P24   :  *pu32W = 1920; *pu32H = 1080; *pu32Frm = 24; break;
        case VO_OUTPUT_1080P25   :  *pu32W = 1920; *pu32H = 1080; *pu32Frm = 25; break;
        case VO_OUTPUT_1080P30   :  *pu32W = 1920; *pu32H = 1080; *pu32Frm = 30; break;
        case VO_OUTPUT_1080P50   :  *pu32W = 1920; *pu32H = 1080; *pu32Frm = 50; break;
        case VO_OUTPUT_1080P60   :  *pu32W = 1920; *pu32H = 1080; *pu32Frm = 60; break;
        case VO_OUTPUT_1024x768_60:  *pu32W = 1024; *pu32H = 768;  *pu32Frm = 60; break;
        case VO_OUTPUT_1280x1024_60: *pu32W = 1280; *pu32H = 1024; *pu32Frm = 60; break;
        case VO_OUTPUT_1366x768_60:  *pu32W = 1366; *pu32H = 768;  *pu32Frm = 60; break;
        case VO_OUTPUT_1440x900_60:  *pu32W = 1440; *pu32H = 900;  *pu32Frm = 60; break;
        case VO_OUTPUT_1280x800_60:  *pu32W = 1280; *pu32H = 800;  *pu32Frm = 60; break;
        case VO_OUTPUT_1600x1200_60: *pu32W = 1600; *pu32H = 1200; *pu32Frm = 60; break;
        case VO_OUTPUT_1680x1050_60: *pu32W = 1680; *pu32H = 1050; *pu32Frm = 60; break;
        case VO_OUTPUT_1920x1200_60: *pu32W = 1920; *pu32H = 1200; *pu32Frm = 60; break;
        case VO_OUTPUT_3840x2160_30: *pu32W = 3840; *pu32H = 2160; *pu32Frm = 30; break;
        case VO_OUTPUT_3840x2160_60: *pu32W = 3840; *pu32H = 2160; *pu32Frm = 60; break;
        case VO_OUTPUT_USER    :     *pu32W = 720;  *pu32H = 576;  *pu32Frm = 25; break;
        default:
            SAMPLE_PRT("vo enIntfSync not support!\n");
            return HI_FAILURE;
    }
    return HI_SUCCESS;
}

HI_S32 SAMPLE_COMM_VO_StartLayer(VO_LAYER VoLayer,const VO_VIDEO_LAYER_ATTR_S *pstLayerAttr)
{
    HI_S32 s32Ret = HI_SUCCESS;
    s32Ret = HI_MPI_VO_SetVideoLayerAttr(VoLayer, pstLayerAttr);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }
#if 0 // ln debug
    s32Ret = HI_MPI_VO_SetVideoLayerPartitionMode(VoLayer,VO_PART_MODE_SINGLE);

    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_VO_SetVideoLayerPartitionMode failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }
#endif

    s32Ret = HI_MPI_VO_EnableVideoLayer(VoLayer);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    return s32Ret;
}

HI_S32 SAMPLE_COMM_VO_StartChn(VO_LAYER VoLayer, SAMPLE_VO_MODE_E enMode)
{
    HI_S32 i;
    HI_S32 s32Ret = HI_SUCCESS;
    HI_U32 u32WndNum = 0;
    HI_U32 u32Square = 0;
    HI_U32 u32Width = 0;
    HI_U32 u32Height = 0;
    VO_CHN_ATTR_S stChnAttr;
    VO_VIDEO_LAYER_ATTR_S stLayerAttr;
    HI_S32 s32ChnFrmRate;

    switch (enMode)
    {
        case VO_MODE_1MUX:
            u32WndNum = 1;
            u32Square = 1;
            break;
        case VO_MODE_4MUX:
            u32WndNum = 4;
            u32Square = 2;
            break;
        case VO_MODE_9MUX:
            u32WndNum = 9;
            u32Square = 3;
            break;
        case VO_MODE_16MUX:
            u32WndNum = 16;
            u32Square = 4;
            break;
        default:
            SAMPLE_PRT("failed with %#x!\n", s32Ret);
            return HI_FAILURE;
    }

    s32Ret = HI_MPI_VO_GetVideoLayerAttr(VoLayer, &stLayerAttr);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }
    u32Width = stLayerAttr.stImageSize.u32Width;
    u32Height = stLayerAttr.stImageSize.u32Height;
    printf("u32Width:%d, u32Square:%d\n", u32Width, u32Square);

    if (stLayerAttr.u32DispFrmRt <= 0)
    {
        s32ChnFrmRate = 30;
    }
    else if (stLayerAttr.u32DispFrmRt > 30)
    {
        s32ChnFrmRate = stLayerAttr.u32DispFrmRt / 2;
    }
    else
    {
        s32ChnFrmRate = stLayerAttr.u32DispFrmRt;
    }

    //for (i=0; i<u32WndNum; i++)
    for (i=0; i<2; i++)
    {
#if 1
        stChnAttr.stRect.s32X       = ALIGN_BACK((u32Width/u32Square) * (i%u32Square), 2);
        stChnAttr.stRect.s32Y       = ALIGN_BACK((u32Height/u32Square) * (i/u32Square), 2);
        stChnAttr.stRect.u32Width   = ALIGN_BACK(u32Width/u32Square, 2);
        stChnAttr.stRect.u32Height  = ALIGN_BACK(u32Height/u32Square, 2);
#else
        stChnAttr.stRect.s32X       = i*960;
        stChnAttr.stRect.s32Y       = 0;
        stChnAttr.stRect.u32Width   = 960;
        stChnAttr.stRect.u32Height  = 1080;
#endif
        stChnAttr.u32Priority       = 0;
        stChnAttr.bDeflicker        = HI_FALSE;

        s32Ret = HI_MPI_VO_SetChnAttr(VoLayer, i, &stChnAttr);
        if (s32Ret != HI_SUCCESS)
        {
            printf("%s(%d):failed with %#x!\n", __FUNCTION__, __LINE__, s32Ret);
            return HI_FAILURE;
        }

        s32Ret = HI_MPI_VO_SetChnFrameRate(VoLayer, i, s32ChnFrmRate);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("failed with %#x!\n", s32Ret);
            return HI_FAILURE;
        }

        s32Ret = HI_MPI_VO_EnableChn(VoLayer, i);
        if (s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("failed with %#x!\n", s32Ret);
            return HI_FAILURE;
        }
    }
    return HI_SUCCESS;
}

HI_S32 SAMPLE_COMM_VO_StartChn_2(VO_LAYER VoLayer)
{
    HI_S32 i;
    HI_S32 s32Ret = HI_SUCCESS;
    HI_U32 u32WndNum = 0;
    HI_U32 u32Square = 0;
    HI_U32 u32Width = 0;
    HI_U32 u32Height = 0;
    VO_CHN_ATTR_S stChnAttr;
    VO_VIDEO_LAYER_ATTR_S stLayerAttr;
    HI_S32 s32ChnFrmRate;

        u32Square = 2;

    s32Ret = HI_MPI_VO_GetVideoLayerAttr(VoLayer, &stLayerAttr);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    u32Width = stLayerAttr.stImageSize.u32Width;
    u32Height = stLayerAttr.stImageSize.u32Height;
    printf("u32Width:%d, height :%d frame_rate = %d\n", u32Width, u32Height, stLayerAttr.u32DispFrmRt);

    if (stLayerAttr.u32DispFrmRt <= 0)
    {
        s32ChnFrmRate = 30;
    }
    else if (stLayerAttr.u32DispFrmRt > 30)
    {
        s32ChnFrmRate = stLayerAttr.u32DispFrmRt / 2;
    }
    else
    {
        s32ChnFrmRate = stLayerAttr.u32DispFrmRt;
    }



    //for (i=0; i<u32WndNum; i++)
    for (i=0; i<2; i++)
    {
        stChnAttr.stRect.s32X       = 960 * i;
        stChnAttr.stRect.s32Y       = 0;
        stChnAttr.stRect.u32Width   = 960;
        stChnAttr.stRect.u32Height  = 1080;
        stChnAttr.u32Priority       = 0;
        stChnAttr.bDeflicker        = HI_FALSE;

        s32Ret = HI_MPI_VO_SetChnAttr(VoLayer, i, &stChnAttr);
        if (s32Ret != HI_SUCCESS)
        {
            printf("%s(%d):failed with %#x!\n", __FUNCTION__, __LINE__, s32Ret);
            return HI_FAILURE;
        }

        s32Ret = HI_MPI_VO_SetChnFrameRate(VoLayer, i, s32ChnFrmRate);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("failed with %#x!\n", s32Ret);
            return HI_FAILURE;
        }

        s32Ret = HI_MPI_VO_EnableChn(VoLayer, i);
        if (s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("failed with %#x!\n", s32Ret);
            return HI_FAILURE;
        }
    }
    return HI_SUCCESS;
}

static HI_VOID SAMPLE_COMM_VO_HdmiConvertSync(VO_INTF_SYNC_E enIntfSync,
    HI_HDMI_VIDEO_FMT_E *penVideoFmt)
{
    switch (enIntfSync)
    {
        case VO_OUTPUT_PAL:
            *penVideoFmt = HI_HDMI_VIDEO_FMT_PAL;
            break;
        case VO_OUTPUT_NTSC:
            *penVideoFmt = HI_HDMI_VIDEO_FMT_NTSC;
            break;
        case VO_OUTPUT_1080P24:
            *penVideoFmt = HI_HDMI_VIDEO_FMT_1080P_24;
            break;
        case VO_OUTPUT_1080P25:
            *penVideoFmt = HI_HDMI_VIDEO_FMT_1080P_25;
            break;
        case VO_OUTPUT_1080P30:
            *penVideoFmt = HI_HDMI_VIDEO_FMT_1080P_30;
            break;
        case VO_OUTPUT_720P50:
            *penVideoFmt = HI_HDMI_VIDEO_FMT_720P_50;
            break;
        case VO_OUTPUT_720P60:
            *penVideoFmt = HI_HDMI_VIDEO_FMT_720P_60;
            break;
        case VO_OUTPUT_1080I50:
            *penVideoFmt = HI_HDMI_VIDEO_FMT_1080i_50;
            break;
        case VO_OUTPUT_1080I60:
            *penVideoFmt = HI_HDMI_VIDEO_FMT_1080i_60;
            break;
        case VO_OUTPUT_1080P50:
            *penVideoFmt = HI_HDMI_VIDEO_FMT_1080P_50;
            break;
        case VO_OUTPUT_1080P60:
            *penVideoFmt = HI_HDMI_VIDEO_FMT_1080P_60;
            break;
        case VO_OUTPUT_576P50:
            *penVideoFmt = HI_HDMI_VIDEO_FMT_576P_50;
            break;
        case VO_OUTPUT_480P60:
            *penVideoFmt = HI_HDMI_VIDEO_FMT_480P_60;
            break;
        case VO_OUTPUT_800x600_60:
            *penVideoFmt = HI_HDMI_VIDEO_FMT_VESA_800X600_60;
            break;
        case VO_OUTPUT_1024x768_60:
            *penVideoFmt = HI_HDMI_VIDEO_FMT_VESA_1024X768_60;
            break;
        case VO_OUTPUT_1280x1024_60:
            *penVideoFmt = HI_HDMI_VIDEO_FMT_VESA_1280X1024_60;
            break;
        case VO_OUTPUT_1366x768_60:
            *penVideoFmt = HI_HDMI_VIDEO_FMT_VESA_1366X768_60;
            break;
        case VO_OUTPUT_1440x900_60:
            *penVideoFmt = HI_HDMI_VIDEO_FMT_VESA_1440X900_60;
            break;
        case VO_OUTPUT_1280x800_60:
            *penVideoFmt = HI_HDMI_VIDEO_FMT_VESA_1280X800_60;
            break;

        default :
            SAMPLE_PRT("Unkonw VO_INTF_SYNC_E value!\n");
            break;
    }

    return;
}

HI_S32 SAMPLE_COMM_VO_HdmiStart(VO_INTF_SYNC_E enIntfSync)
{
    HI_HDMI_ATTR_S      stAttr;
    HI_HDMI_VIDEO_FMT_E enVideoFmt;
    HI_HDMI_INIT_PARA_S stHdmiPara;

    SAMPLE_COMM_VO_HdmiConvertSync(enIntfSync, &enVideoFmt);

    stHdmiPara.pfnHdmiEventCallback = NULL;
    stHdmiPara.pCallBackArgs = NULL;
    stHdmiPara.enForceMode = HI_HDMI_FORCE_HDMI;
    HI_MPI_HDMI_Init(&stHdmiPara);

    HI_MPI_HDMI_Open(HI_HDMI_ID_0);

    HI_MPI_HDMI_GetAttr(HI_HDMI_ID_0, &stAttr);

    stAttr.bEnableHdmi = HI_TRUE;

    stAttr.bEnableVideo = HI_TRUE;
    stAttr.enVideoFmt = enVideoFmt;

    stAttr.enVidOutMode = HI_HDMI_VIDEO_MODE_YCBCR444;
    stAttr.enDeepColorMode = HI_HDMI_DEEP_COLOR_OFF;
    stAttr.bxvYCCMode = HI_FALSE;

    stAttr.bEnableAudio = HI_FALSE;
    stAttr.enSoundIntf = HI_HDMI_SND_INTERFACE_I2S;
    stAttr.bIsMultiChannel = HI_FALSE;

    stAttr.enBitDepth = HI_HDMI_BIT_DEPTH_16;

    stAttr.bEnableAviInfoFrame = HI_TRUE;
    stAttr.bEnableAudInfoFrame = HI_TRUE;
    stAttr.bEnableSpdInfoFrame = HI_FALSE;
    stAttr.bEnableMpegInfoFrame = HI_FALSE;

    stAttr.bDebugFlag = HI_FALSE;
    stAttr.bHDCPEnable = HI_FALSE;

    stAttr.b3DEnable = HI_FALSE;

    HI_MPI_HDMI_SetAttr(HI_HDMI_ID_0, &stAttr);

    HI_MPI_HDMI_Start(HI_HDMI_ID_0);

    printf("HDMI start success.\n");
    return HI_SUCCESS;
}


HI_S32 SAMPLE_COMM_VO_BindVpss(VO_LAYER VoLayer,VO_CHN VoChn,VPSS_GRP VpssGrp,VPSS_CHN VpssChn)
{
    HI_S32 s32Ret = HI_SUCCESS;
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = HI_ID_VPSS;
    stSrcChn.s32DevId = VpssGrp;
    stSrcChn.s32ChnId = VpssChn;

    stDestChn.enModId = HI_ID_VOU;
    stDestChn.s32DevId = VoLayer;
    stDestChn.s32ChnId = VoChn;

    s32Ret = HI_MPI_SYS_Bind(&stSrcChn, &stDestChn);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    return s32Ret;
}

HI_S32 SAMPLE_COMM_VO_StopChn(VO_LAYER VoLayer, SAMPLE_VO_MODE_E enMode)
{
    HI_S32 i;
    HI_S32 s32Ret = HI_SUCCESS;
    HI_U32 u32WndNum = 0;

    switch (enMode)
    {
        case VO_MODE_1MUX:
        {
            u32WndNum = 1;
            break;
        }
        case VO_MODE_4MUX:
        {
            u32WndNum = 4;
            break;
        }
        case VO_MODE_9MUX:
        {
            u32WndNum = 9;
            break;
        }
        case VO_MODE_16MUX:
        {
            u32WndNum = 16;
            break;
        }

        default:
            SAMPLE_PRT("failed with %#x!\n", s32Ret);
            return HI_FAILURE;
    }

    for (i=0; i<u32WndNum; i++)
    {
        s32Ret = HI_MPI_VO_DisableChn(VoLayer, i);
        if (s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("failed with %#x!\n", s32Ret);
            return HI_FAILURE;
        }
    }
    return s32Ret;
}

HI_S32 SAMPLE_COMM_VO_StopLayer(VO_LAYER VoLayer)
{
    HI_S32 s32Ret = HI_SUCCESS;

    s32Ret = HI_MPI_VO_DisableVideoLayer(VoLayer);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }
    return s32Ret;
}

HI_S32 SAMPLE_COMM_VO_StopDev(VO_DEV VoDev)
{
    HI_S32 s32Ret = HI_SUCCESS;

    s32Ret = HI_MPI_VO_Disable(VoDev);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }
    return s32Ret;
}

HI_S32 SAMPLE_COMM_VO_HdmiStop(HI_VOID)
{
    HI_MPI_HDMI_Stop(HI_HDMI_ID_0);
    HI_MPI_HDMI_Close(HI_HDMI_ID_0);
    HI_MPI_HDMI_DeInit();

    return HI_SUCCESS;
}

HI_S32 SAMPLE_COMM_VO_UnBindVpss(VO_LAYER VoLayer,VO_CHN VoChn,VPSS_GRP VpssGrp,VPSS_CHN VpssChn)
{
    HI_S32 s32Ret = HI_SUCCESS;
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = HI_ID_VPSS;
    stSrcChn.s32DevId = VpssGrp;
    stSrcChn.s32ChnId = VpssChn;

    stDestChn.enModId = HI_ID_VOU;
    stDestChn.s32DevId = VoLayer;
    stDestChn.s32ChnId = VoChn;

    s32Ret = HI_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }
    return s32Ret;
}


/*****************************************************************************
* function : Vi chn unbind vpss group
*****************************************************************************/
HI_S32 SAMPLE_COMM_VI_UnBindVpss(SAMPLE_VI_MODE_E enViMode)
{
    HI_S32 i, j, s32Ret;
    VPSS_GRP VpssGrp;
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;
    SAMPLE_VI_PARAM_S stViParam;
    VI_DEV ViDev;
    VI_CHN ViChn;

    s32Ret = SAMPLE_COMM_VI_Mode2Param(enViMode, &stViParam);
    if (HI_SUCCESS !=s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_Mode2Param failed!\n");
        return HI_FAILURE;
    }

    VpssGrp = 0;
    for (i=0; i<stViParam.s32ViDevCnt; i++)
    {
        ViDev = i * stViParam.s32ViDevInterval;

        for (j=0; j<stViParam.s32ViChnCnt; j++)
        {
            ViChn = j * stViParam.s32ViChnInterval;

            stSrcChn.enModId = HI_ID_VIU;
            stSrcChn.s32DevId = ViDev;
            stSrcChn.s32ChnId = ViChn;

            stDestChn.enModId = HI_ID_VPSS;
            stDestChn.s32DevId = VpssGrp;
            stDestChn.s32ChnId = 0;

            s32Ret = HI_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
            if (s32Ret != HI_SUCCESS)
            {
                SAMPLE_PRT("failed with %#x!\n", s32Ret);
                return HI_FAILURE;
            }

            VpssGrp ++;
        }
    }
    return HI_SUCCESS;
}

/*****************************************************************************
* function : disable vi dev
*****************************************************************************/
HI_S32 SAMPLE_COMM_VPSS_Stop(HI_S32 s32GrpCnt, HI_S32 s32ChnCnt)
{
    HI_S32 i, j;
    HI_S32 s32Ret = HI_SUCCESS;
    VPSS_GRP VpssGrp;
    VPSS_CHN VpssChn;

    for(i=0; i<s32GrpCnt; i++)
    {
        VpssGrp = i;
        s32Ret = HI_MPI_VPSS_StopGrp(VpssGrp);
        if (s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("failed with %#x!\n", s32Ret);
            return HI_FAILURE;
        }
        for(j=0; j<s32ChnCnt; j++)
        {
            VpssChn = j;
            s32Ret = HI_MPI_VPSS_DisableChn(VpssGrp, VpssChn);
            if (s32Ret != HI_SUCCESS)
            {
                SAMPLE_PRT("failed with %#x!\n", s32Ret);
                return HI_FAILURE;
            }
        }

        s32Ret = HI_MPI_VPSS_DestroyGrp(VpssGrp);
        if (s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("failed with %#x!\n", s32Ret);
            return HI_FAILURE;
        }
    }

    return HI_SUCCESS;
}

/******************************************************************************
* function : vb exit & MPI system exit
******************************************************************************/
HI_VOID SAMPLE_COMM_SYS_Exit(void)
{

    HI_S32 i;

    HI_MPI_SYS_Exit();
    for(i=0;i<VB_MAX_USER;i++)
    {
         HI_MPI_VB_ExitModCommPool(i);
    }
    for(i=0; i<VB_MAX_POOLS; i++)
    {
         HI_MPI_VB_DestroyPool(i);
    }
    HI_MPI_VB_Exit();
    return;
}

/*****************************************************************************
* function : stop vi accroding to product type
*****************************************************************************/
HI_S32 SAMPLE_COMM_VI_Stop(SAMPLE_VI_MODE_E enViMode)
{
    VI_DEV ViDev;
    VI_CHN ViChn;
    HI_S32 i;
    HI_S32 s32Ret;
    SAMPLE_VI_PARAM_S stViParam;

    /*** get parameter from Sample_Vi_Mode ***/
    s32Ret = SAMPLE_COMM_VI_Mode2Param(enViMode, &stViParam);
    if (HI_SUCCESS !=s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VI_Mode2Param failed!\n");
        return HI_FAILURE;
    }

    /*** Stop VI Chn ***/
    for(i=0;i<stViParam.s32ViChnCnt;i++)
    {
        /* Stop vi phy-chn */
        ViChn = i * stViParam.s32ViChnInterval;
        s32Ret = HI_MPI_VI_DisableChn(ViChn);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("SAMPLE_COMM_VI_StopChn failed with %#x\n",s32Ret);
            return HI_FAILURE;
        }
    }

    /*** Stop VI Dev ***/
    for(i=0; i<stViParam.s32ViDevCnt; i++)
    {
        ViDev = i * stViParam.s32ViDevInterval;
        s32Ret = HI_MPI_VI_DisableDev(ViDev);
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("SAMPLE_COMM_VI_StopDev failed with %#x\n", s32Ret);
            return HI_FAILURE;
        }
    }

    return HI_SUCCESS;
}

HI_S32 SAMPLE_COMM_VO_BindVi(VO_LAYER VoLayer, VO_CHN VoChn, VI_CHN ViChn)
{
    MPP_CHN_S stSrcChn, stDestChn;

    stSrcChn.enModId    = HI_ID_VIU;
    stSrcChn.s32DevId   = 0;
    stSrcChn.s32ChnId   = ViChn;

    stDestChn.enModId   = HI_ID_VOU;
    stDestChn.s32ChnId  = VoChn;
    stDestChn.s32DevId  = VoLayer;

    return HI_MPI_SYS_Bind(&stSrcChn, &stDestChn);
}


/******************************************************************************
* funciton : Start venc stream mode (h264, mjpeg)
* note      : rate control parameter need adjust, according your case.
******************************************************************************/
HI_S32 SAMPLE_COMM_VENC_Start(VENC_CHN VencChn, PAYLOAD_TYPE_E enType, VIDEO_NORM_E enNorm, PIC_SIZE_E enSize, SAMPLE_RC_E enRcMode,HI_U32 u32Profile)
{
    HI_S32 s32Ret;
    VENC_CHN_ATTR_S stVencChnAttr;
    VENC_ATTR_H264_S stH264Attr;
    VENC_ATTR_H264_CBR_S    stH264Cbr;
    VENC_ATTR_H264_VBR_S    stH264Vbr;
    VENC_ATTR_H264_FIXQP_S  stH264FixQp;
    VENC_ATTR_MJPEG_S stMjpegAttr;
    VENC_ATTR_MJPEG_FIXQP_S stMjpegeFixQp;
    VENC_ATTR_JPEG_S stJpegAttr;
    SIZE_S stPicSize;

    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enNorm, enSize, &stPicSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Get picture size failed!\n");
        return HI_FAILURE;
    }

    /******************************************
     step 1:  Create Venc Channel
    ******************************************/
    stVencChnAttr.stVeAttr.enType = enType;
    switch(enType)
    {
        case PT_H264:
        {
            stH264Attr.u32MaxPicWidth  = stPicSize.u32Width;
            stH264Attr.u32MaxPicHeight = stPicSize.u32Height;
            stH264Attr.u32PicWidth     = stPicSize.u32Width;/*the picture width*/
            stH264Attr.u32PicHeight    = stPicSize.u32Height;/*the picture height*/
            stH264Attr.u32BufSize      = stPicSize.u32Width * stPicSize.u32Height * 2;/*stream buffer size*/
            stH264Attr.u32Profile      = u32Profile;/*0: baseline; 1:MP; 2:HP 3:svc-t */
            stH264Attr.bByFrame        = HI_TRUE;/*get stream mode is slice mode or frame mode?*/
            memcpy(&stVencChnAttr.stVeAttr.stAttrH264e, &stH264Attr, sizeof(VENC_ATTR_H264_S));

            if(SAMPLE_RC_CBR == enRcMode)
            {
                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
                stH264Cbr.u32Gop            = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stH264Cbr.u32StatTime       = 1; /* stream rate statics time(s) */
                stH264Cbr.u32SrcFrmRate      = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;/* input (vi) frame rate */
                stH264Cbr.fr32DstFrmRate = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;/* target frame rate */
                switch (enSize)
                {
                  case PIC_QCIF:
                        stH264Cbr.u32BitRate = 256; /* average bit rate */
                        break;
                  case PIC_QVGA:    /* 320 * 240 */
                  case PIC_CIF:
                        stH264Cbr.u32BitRate = 256;
                        break;
                  case PIC_D1:
                  case PIC_VGA:	   /* 640 * 480 */
                        stH264Cbr.u32BitRate = 1024*2;
                        break;
                  case PIC_HD720:   /* 1280 * 720 */
                       stH264Cbr.u32BitRate = 1024*3;
                       break;
                  case PIC_HD1080:  /* 1920 * 1080 */
                       stH264Cbr.u32BitRate = 1024*1;
                       break;
                  default :
                        stH264Cbr.u32BitRate = 1024*4;
                        break;
                }

                stH264Cbr.u32FluctuateLevel = 0; /* average bit rate */
                memcpy(&stVencChnAttr.stRcAttr.stAttrH264Cbr, &stH264Cbr, sizeof(VENC_ATTR_H264_CBR_S));
            }
            else if (SAMPLE_RC_FIXQP == enRcMode)
            {
                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264FIXQP;
                stH264FixQp.u32Gop = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stH264FixQp.u32SrcFrmRate = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stH264FixQp.fr32DstFrmRate = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stH264FixQp.u32IQp = 20;
                stH264FixQp.u32PQp = 23;
                memcpy(&stVencChnAttr.stRcAttr.stAttrH264FixQp, &stH264FixQp,sizeof(VENC_ATTR_H264_FIXQP_S));
            }
            else if (SAMPLE_RC_VBR == enRcMode)
            {
                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_H264VBR;
                stH264Vbr.u32Gop = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stH264Vbr.u32StatTime = 1;
                stH264Vbr.u32SrcFrmRate = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stH264Vbr.fr32DstFrmRate = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stH264Vbr.u32MinQp = 10;
                stH264Vbr.u32MaxQp = 40;
                switch (enSize)
                {
                    case PIC_QCIF:
                       stH264Vbr.u32MaxBitRate= 256*3; /* average bit rate */
                       break;
                    case PIC_QVGA:    /* 320 * 240 */
                    case PIC_CIF:
                       stH264Vbr.u32MaxBitRate = 512*3;
                       break;
                    case PIC_D1:
                    case PIC_VGA:	   /* 640 * 480 */
                       stH264Vbr.u32MaxBitRate = 1024*2;
                        break;
                    case PIC_HD720:   /* 1280 * 720 */
                       stH264Vbr.u32MaxBitRate = 1024*3;
                       break;
                    case PIC_HD1080:  /* 1920 * 1080 */
                       stH264Vbr.u32MaxBitRate = 1024*6;
                       break;
                    default :
                        stH264Vbr.u32MaxBitRate = 1024*4*3;
                        break;
                }
                memcpy(&stVencChnAttr.stRcAttr.stAttrH264Vbr, &stH264Vbr, sizeof(VENC_ATTR_H264_VBR_S));
            }
            else
            {
                return HI_FAILURE;
            }
        }
        break;

        case PT_MJPEG:
        {
            stMjpegAttr.u32MaxPicWidth = stPicSize.u32Width;
            stMjpegAttr.u32MaxPicHeight = stPicSize.u32Height;
            stMjpegAttr.u32PicWidth = stPicSize.u32Width;
            stMjpegAttr.u32PicHeight = stPicSize.u32Height;
            stMjpegAttr.u32BufSize = stPicSize.u32Width * stPicSize.u32Height * 2;
            stMjpegAttr.bByFrame = HI_TRUE;  /*get stream mode is field mode  or frame mode*/
            memcpy(&stVencChnAttr.stVeAttr.stAttrMjpeg, &stMjpegAttr, sizeof(VENC_ATTR_MJPEG_S));

            if(SAMPLE_RC_FIXQP == enRcMode)
            {
                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_MJPEGFIXQP;
                stMjpegeFixQp.u32Qfactor        = 90;
                stMjpegeFixQp.u32SrcFrmRate      = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stMjpegeFixQp.fr32DstFrmRate = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                memcpy(&stVencChnAttr.stRcAttr.stAttrMjpegeFixQp, &stMjpegeFixQp,
                       sizeof(VENC_ATTR_MJPEG_FIXQP_S));
            }
            else if (SAMPLE_RC_CBR == enRcMode)
            {
                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_MJPEGCBR;
                stVencChnAttr.stRcAttr.stAttrMjpegeCbr.u32StatTime       = 1;
                stVencChnAttr.stRcAttr.stAttrMjpegeCbr.u32SrcFrmRate      = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stVencChnAttr.stRcAttr.stAttrMjpegeCbr.fr32DstFrmRate = (VIDEO_ENCODING_MODE_PAL== enNorm)?25:30;
                stVencChnAttr.stRcAttr.stAttrMjpegeCbr.u32FluctuateLevel = 0;
                switch (enSize)
                {
                    case PIC_QCIF:
                        stVencChnAttr.stRcAttr.stAttrMjpegeCbr.u32BitRate = 384*3; /* average bit rate */
                       break;
                    case PIC_QVGA:    /* 320 * 240 */
                    case PIC_CIF:
                       stVencChnAttr.stRcAttr.stAttrMjpegeCbr.u32BitRate = 768*3;
                        break;
                    case PIC_D1:
                    case PIC_VGA:	   /* 640 * 480 */
                       stVencChnAttr.stRcAttr.stAttrMjpegeCbr.u32BitRate = 1024*3*3;
                        break;
                    case PIC_HD720:   /* 1280 * 720 */
                       stVencChnAttr.stRcAttr.stAttrMjpegeCbr.u32BitRate = 1024*5*3;
                       break;
                    case PIC_HD1080:  /* 1920 * 1080 */
                       stVencChnAttr.stRcAttr.stAttrMjpegeCbr.u32BitRate = 1024*10*3;
                       break;
                    default :
                        stVencChnAttr.stRcAttr.stAttrMjpegeCbr.u32BitRate = 1024*7*3;
                        break;
                }
            }
            else if (SAMPLE_RC_VBR == enRcMode)
            {
                stVencChnAttr.stRcAttr.enRcMode = VENC_RC_MODE_MJPEGVBR;
                stVencChnAttr.stRcAttr.stAttrMjpegeVbr.u32StatTime = 1;
                stVencChnAttr.stRcAttr.stAttrMjpegeVbr.u32SrcFrmRate = (VIDEO_ENCODING_MODE_PAL == enNorm)?25:30;
                stVencChnAttr.stRcAttr.stAttrMjpegeVbr.fr32DstFrmRate = 5;
                stVencChnAttr.stRcAttr.stAttrMjpegeVbr.u32MinQfactor = 50;
                stVencChnAttr.stRcAttr.stAttrMjpegeVbr.u32MaxQfactor = 95;
                switch (enSize)
                {
                    case PIC_QCIF:
                        stVencChnAttr.stRcAttr.stAttrMjpegeVbr.u32MaxBitRate= 256*3; /* average bit rate */
                       break;
                    case PIC_QVGA:    /* 320 * 240 */
                    case PIC_CIF:
                       stVencChnAttr.stRcAttr.stAttrMjpegeVbr.u32MaxBitRate = 512*3;
                        break;
                    case PIC_D1:
                    case PIC_VGA:	   /* 640 * 480 */
                       stVencChnAttr.stRcAttr.stAttrMjpegeVbr.u32MaxBitRate = 1024*2*3;
                        break;
                    case PIC_HD720:   /* 1280 * 720 */
                       stVencChnAttr.stRcAttr.stAttrMjpegeVbr.u32MaxBitRate = 1024*3*3;
                       break;
                    case PIC_HD1080:  /* 1920 * 1080 */
                       stVencChnAttr.stRcAttr.stAttrMjpegeVbr.u32MaxBitRate = 1024*6*3;
                       break;
                    default :
                       stVencChnAttr.stRcAttr.stAttrMjpegeVbr.u32MaxBitRate = 1024*4*3;
                       break;
                }
            }
            else
            {
                SAMPLE_PRT("cann't support other mode in this version!\n");
                return HI_FAILURE;
            }
        }
        break;

        case PT_JPEG:
            stJpegAttr.u32PicWidth  = stPicSize.u32Width;
            stJpegAttr.u32PicHeight = stPicSize.u32Height;
            stJpegAttr.u32BufSize = stPicSize.u32Width * stPicSize.u32Height * 2;
            stJpegAttr.bByFrame = HI_TRUE;/*get stream mode is field mode  or frame mode*/
            stJpegAttr.bSupportDCF = HI_FALSE;
            memcpy(&stVencChnAttr.stVeAttr.stAttrJpeg, &stJpegAttr, sizeof(stJpegAttr));
            break;
        default:
            return HI_ERR_VENC_NOT_SUPPORT;
    }

    s32Ret = HI_MPI_VENC_CreateChn(VencChn, &stVencChnAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VENC_CreateChn [%d] faild with %#x!\n",\
                VencChn, s32Ret);
        return s32Ret;
    }

    /******************************************
     step 2:  Start Recv Venc Pictures
    ******************************************/
    s32Ret = HI_MPI_VENC_StartRecvPic(VencChn);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("HI_MPI_VENC_StartRecvPic faild with%#x!\n", s32Ret);
        return HI_FAILURE;
    }

    return HI_SUCCESS;

}

/******************************************************************************
* function : venc bind vo
******************************************************************************/
HI_S32 SAMPLE_COMM_VENC_BindVo(VO_DEV VoDev,VO_CHN VoChn,VENC_CHN VeChn)
{
    HI_S32 s32Ret = HI_SUCCESS;
    MPP_CHN_S stSrcChn;
    MPP_CHN_S stDestChn;

    stSrcChn.enModId = HI_ID_VOU;
    stSrcChn.s32DevId = VoDev;
    stSrcChn.s32ChnId = VoChn;

    stDestChn.enModId = HI_ID_VENC;
    stDestChn.s32DevId = 0;
    stDestChn.s32ChnId = VeChn;

    s32Ret = HI_MPI_SYS_Bind(&stSrcChn, &stDestChn);
    if (s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("failed with %#x!\n", s32Ret);
        return HI_FAILURE;
    }

    return s32Ret;
}
#if 0
void *get_stream_thread(void *data)
{
    int i = 0;
    int s32Ret;
    /* decide the stream file name, and open file to save stream */
    int VencChn = i;
    VENC_CHN_ATTR_S stVencChnAttr;
    PAYLOAD_TYPE_E enPayLoadType[VENC_MAX_CHN_NUM];
    HI_CHAR aszFileName[VENC_MAX_CHN_NUM][64];
    FILE *pFile;

    s32Ret = HI_MPI_VENC_GetChnAttr(VencChn, &stVencChnAttr);
    if(s32Ret != HI_SUCCESS)
    {
        SAMPLE_PRT("HI_MPI_VENC_GetChnAttr chn[%d] failed with %#x!\n", \
               VencChn, s32Ret);
        return NULL;
    }
    enPayLoadType[i] = stVencChnAttr.stVeAttr.enType;

    sprintf(aszFileName[i], "stream_chn%d.h264", i);
    pFile[i] = fopen(aszFileName[i], "wb");
    if (!pFile[i])
    {
        SAMPLE_PRT("open file[%s] failed!\n",
               aszFileName[i]);
        return NULL;
    }

    /* Set Venc Fd. */
    VencFd[i] = HI_MPI_VENC_GetFd(i);
    if (VencFd[i] < 0)
    {
        SAMPLE_PRT("HI_MPI_VENC_GetFd failed with %#x!\n",
               VencFd[i]);
        return NULL;
    }
    if (maxfd <= VencFd[i])
    {
        maxfd = VencFd[i];
    }
}
#endif

typedef struct sample_venc_getstream_s
{
     HI_BOOL bThreadStart;
     HI_S32  s32Cnt;
}SAMPLE_VENC_GETSTREAM_PARA_S;

static SAMPLE_VENC_GETSTREAM_PARA_S gs_stPara;
static pthread_t gs_VencPid;

/******************************************************************************
* funciton : get file postfix according palyload_type.
******************************************************************************/
HI_S32 SAMPLE_COMM_VENC_GetFilePostfix(PAYLOAD_TYPE_E enPayload, char *szFilePostfix)
{
    if (PT_H264 == enPayload)
    {
        strcpy(szFilePostfix, ".h264");
    }
    else if (PT_JPEG == enPayload)
    {
        strcpy(szFilePostfix, ".jpg");
    }
    else if (PT_MJPEG == enPayload)
    {
        strcpy(szFilePostfix, ".mjp");
    }
    else if (PT_MP4VIDEO == enPayload)
    {
        strcpy(szFilePostfix, ".mp4");
    }
    else
    {
        SAMPLE_PRT("payload type err!\n");
        return HI_FAILURE;
    }
    return HI_SUCCESS;
}

/******************************************************************************
* funciton : save H264 stream
******************************************************************************/
HI_S32 SAMPLE_COMM_VENC_SaveH264(FILE* fpH264File, VENC_STREAM_S *pstStream)
{
    HI_S32 i;

    for (i = 0; i < pstStream->u32PackCount; i++)
    {
        fwrite(pstStream->pstPack[i].pu8Addr+pstStream->pstPack[i].u32Offset,
               pstStream->pstPack[i].u32Len-pstStream->pstPack[i].u32Offset, 1, fpH264File);

        fflush(fpH264File);
    }


    return HI_SUCCESS;
}


/******************************************************************************
* funciton : save stream
******************************************************************************/
HI_S32 SAMPLE_COMM_VENC_SaveStream(PAYLOAD_TYPE_E enType,FILE *pFd, VENC_STREAM_S *pstStream)
{
    HI_S32 s32Ret;

    if (PT_H264 == enType)
    {
        s32Ret = SAMPLE_COMM_VENC_SaveH264(pFd, pstStream);
    }
    else if (PT_MJPEG == enType)
    {
        //s32Ret = SAMPLE_COMM_VENC_SaveMJpeg(pFd, pstStream);
    }
    else
    {
        return HI_FAILURE;
    }
    return s32Ret;
}


/******************************************************************************
* funciton : get stream from each channels and save them
******************************************************************************/
HI_VOID* SAMPLE_COMM_VENC_GetVencStreamProc(HI_VOID *p)
{
    info_msg("SAMPLE_COMM_VENC_GetVencStreamProc\n");
    HI_S32 i;
    HI_S32 s32ChnTotal;
    VENC_CHN_ATTR_S stVencChnAttr;
    SAMPLE_VENC_GETSTREAM_PARA_S *pstPara;
    HI_S32 maxfd = 0;
    struct timeval TimeoutVal;
    fd_set read_fds;
    HI_S32 VencFd[VENC_MAX_CHN_NUM];
    HI_CHAR aszFileName[VENC_MAX_CHN_NUM][64];
    FILE *pFile[VENC_MAX_CHN_NUM];
    char szFilePostfix[10];
    VENC_CHN_STAT_S stStat;
    VENC_STREAM_S stStream;
    HI_S32 s32Ret;
    VENC_CHN VencChn;
    PAYLOAD_TYPE_E enPayLoadType[VENC_MAX_CHN_NUM];

    pstPara = (SAMPLE_VENC_GETSTREAM_PARA_S*)p;
    s32ChnTotal = pstPara->s32Cnt;

    /******************************************
     step 1:  check & prepare save-file & venc-fd
    ******************************************/
    if (s32ChnTotal >= VENC_MAX_CHN_NUM)
    {
        SAMPLE_PRT("input count invaild\n");
        return NULL;
    }
    for (i = 0; i < s32ChnTotal; i++)
    {
        /* decide the stream file name, and open file to save stream */
        VencChn = i;
        s32Ret = HI_MPI_VENC_GetChnAttr(VencChn, &stVencChnAttr);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("HI_MPI_VENC_GetChnAttr chn[%d] failed with %#x!\n", \
                   VencChn, s32Ret);
            return NULL;
        }
        enPayLoadType[i] = stVencChnAttr.stVeAttr.enType;

        s32Ret = SAMPLE_COMM_VENC_GetFilePostfix(enPayLoadType[i], szFilePostfix);
        if(s32Ret != HI_SUCCESS)
        {
            SAMPLE_PRT("SAMPLE_COMM_VENC_GetFilePostfix [%d] failed with %#x!\n", \
                   stVencChnAttr.stVeAttr.enType, s32Ret);
            return NULL;
        }
        sprintf(aszFileName[i], "stream_chn%d%s", i, szFilePostfix);
        pFile[i] = fopen(aszFileName[i], "wb");
        if (!pFile[i])
        {
            SAMPLE_PRT("open file[%s] failed!\n",
                   aszFileName[i]);
            return NULL;
        }

        /* Set Venc Fd. */
        VencFd[i] = HI_MPI_VENC_GetFd(i);
        if (VencFd[i] < 0)
        {
            SAMPLE_PRT("HI_MPI_VENC_GetFd failed with %#x!\n",
                   VencFd[i]);
            return NULL;
        }
        if (maxfd <= VencFd[i])
        {
            maxfd = VencFd[i];
        }
    }

    /******************************************
     step 2:  Start to get streams of each channel.
    ******************************************/
    while (HI_TRUE == pstPara->bThreadStart)
    {
        FD_ZERO(&read_fds);
        for (i = 0; i < s32ChnTotal; i++)
        {
            FD_SET(VencFd[i], &read_fds);
        }

        TimeoutVal.tv_sec  = 2;
        TimeoutVal.tv_usec = 0;
        s32Ret = select(maxfd + 1, &read_fds, NULL, NULL, &TimeoutVal);
        if (s32Ret < 0)
        {
            SAMPLE_PRT("select failed!\n");
            break;
        }
        else if (s32Ret == 0)
        {
            SAMPLE_PRT("get venc stream time out, exit thread\n");
            continue;
        }
        else
        {
            for (i = 0; i < s32ChnTotal; i++)
            {
                if (FD_ISSET(VencFd[i], &read_fds))
                {
                    /*******************************************************
                     step 2.1 : query how many packs in one-frame stream.
                    *******************************************************/
                    memset(&stStream, 0, sizeof(stStream));
                    s32Ret = HI_MPI_VENC_Query(i, &stStat);
                    if (HI_SUCCESS != s32Ret)
                    {
                        SAMPLE_PRT("HI_MPI_VENC_Query chn[%d] failed with %#x!\n", i, s32Ret);
                        break;
                    }

                    /*******************************************************
                        step 2.2 : suggest to check both u32CurPacks and u32LeftStreamFrames at the same time,for example:
                       if(0 == stStat.u32CurPacks || 0 == stStat.u32LeftStreamFrames)
                        {
                              SAMPLE_PRT("NOTE: Current  frame is NULL!\n");
                              continue;
                        }
                    *******************************************************/
                      if(0 == stStat.u32CurPacks)
                      {

                          SAMPLE_PRT("NOTE: Current  frame is NULL!\n");
                          continue;
                      }
                    /*******************************************************
                     step 2.3 : malloc corresponding number of pack nodes.
                    *******************************************************/
                    stStream.pstPack = (VENC_PACK_S*)malloc(sizeof(VENC_PACK_S) * stStat.u32CurPacks);
                    if (NULL == stStream.pstPack)
                    {
                        SAMPLE_PRT("malloc stream pack failed!\n");
                        break;
                    }

                    /*******************************************************
                     step 2.4 : call mpi to get one-frame stream
                    *******************************************************/
                    stStream.u32PackCount = stStat.u32CurPacks;

                    s32Ret = HI_MPI_VENC_GetStream(i, &stStream, HI_TRUE);
                    if (HI_SUCCESS != s32Ret)
                    {
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        SAMPLE_PRT("HI_MPI_VENC_GetStream failed with %#x!\n", \
                               s32Ret);
                        break;
                    }

                    /*******************************************************
                     step 2.5 : save frame to file
                    *******************************************************/
                    s32Ret = SAMPLE_COMM_VENC_SaveStream(enPayLoadType[i], pFile[i], &stStream);
                    if (HI_SUCCESS != s32Ret)
                    {
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        SAMPLE_PRT("save stream failed!\n");
                        break;
                    }
#if 1
                    // ln debug
                    pthread_mutex_lock(&compose_info.mutex);
                    if(compose_info.translate_venc_stream){
                       compose_info.translate_venc_stream(i,  &stStream);
                    }
                    pthread_mutex_unlock(&compose_info.mutex);
#endif


                    /*******************************************************
                     step 2.6 : release stream
                    *******************************************************/
                    s32Ret = HI_MPI_VENC_ReleaseStream(i, &stStream);
                    if (HI_SUCCESS != s32Ret)
                    {
                        free(stStream.pstPack);
                        stStream.pstPack = NULL;
                        break;
                    }
                    /*******************************************************
                     step 2.7 : free pack nodes
                    *******************************************************/
                    free(stStream.pstPack);
                    stStream.pstPack = NULL;
                }
            }
        }
    }

    /*******************************************************
    * step 3 : close save-file
    *******************************************************/
    for (i = 0; i < s32ChnTotal; i++)
    {
        fclose(pFile[i]);
    }

    return NULL;
}


/******************************************************************************
* funciton : start get venc stream process thread
******************************************************************************/
HI_S32 SAMPLE_COMM_VENC_StartGetStream(HI_S32 s32Cnt)
{
    gs_stPara.bThreadStart = HI_TRUE;
    gs_stPara.s32Cnt = s32Cnt;

    return pthread_create(&gs_VencPid, 0, SAMPLE_COMM_VENC_GetVencStreamProc, (HI_VOID*)&gs_stPara);
}

int hisi_video_mem_init()
{
    VB_CONF_S stVbConf;
    HI_U32 u32BlkSize;
    HI_U32 u32ViChnCnt = 4;
    VIDEO_NORM_E enNorm = VIDEO_ENCODING_MODE_PAL;
    HI_S32 s32Ret = HI_SUCCESS;
    /******************************************
     step  1: init variable
    ******************************************/
    memset(&stVbConf,0,sizeof(VB_CONF_S));
    u32BlkSize = SAMPLE_COMM_SYS_CalcPicVbBlkSize(enNorm,\
                PIC_HD1080, SAMPLE_PIXEL_FORMAT, SAMPLE_SYS_ALIGN_WIDTH,COMPRESS_MODE_SEG);
    stVbConf.u32MaxPoolCnt = 128;

    /* video buffer*/
    //todo: vb=15
    stVbConf.astCommPool[0].u32BlkSize = u32BlkSize;
    stVbConf.astCommPool[0].u32BlkCnt = u32ViChnCnt * 8;

    /******************************************
     step 2: mpp system init.
    ******************************************/
    s32Ret = SAMPLE_COMM_SYS_Init(&stVbConf);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("system init failed with %d!\n", s32Ret);
        exit(-1);
    }
    return 0;
}

/******************************************************************************
* function : VI(D1: 8 windows) -> VPSS -> HD0(1080P@60) -> WBC -> SD0(D1)
******************************************************************************/


HI_S32 start_mpi_video_stream(HI_VOID)
{
    SAMPLE_VI_MODE_E enViMode = SAMPLE_VI_MODE_8_1080P;

    VIDEO_NORM_E enNorm = VIDEO_ENCODING_MODE_PAL;

    HI_S32 s32VpssGrpCnt = 4;

    VPSS_GRP VpssGrp;
    VPSS_GRP_ATTR_S stGrpAttr;
    VPSS_CHN VpssChn_VoHD0 = VPSS_CHN0;
    VO_DEV VoDev;
    VO_LAYER VoLayer;
    VO_CHN VoChn;
    VO_PUB_ATTR_S stVoPubAttr;
    VO_VIDEO_LAYER_ATTR_S stLayerAttr;
    SAMPLE_VO_MODE_E enVoMode;

    HI_S32 i;
    HI_S32 s32Ret = HI_SUCCESS;
    HI_CHAR ch;
    SIZE_S stSize;
    HI_U32 u32WndNum;


    compose_info_init();
    /******************************************
     step 3: start vi dev & chn
    ******************************************/
    s32Ret = SAMPLE_COMM_VI_Start(enViMode, enNorm);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("start vi failed!\n");
        goto END_8X1080P_0;
    }

    /******************************************
     step 4: start vpss and vi bind vpss
    ******************************************/
#if 1
    s32Ret = SAMPLE_COMM_SYS_GetPicSize(enNorm, PIC_HD1080, &stSize);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_SYS_GetPicSize failed!\n");
        goto END_8X1080P_0;
    }

    memset(&stGrpAttr,0,sizeof(VPSS_GRP_ATTR_S));
    stGrpAttr.u32MaxW = stSize.u32Width;
    stGrpAttr.u32MaxH = stSize.u32Height;
    stGrpAttr.bNrEn = HI_TRUE;
    //stGrpAttr.bNrEn = HI_FALSE;
    stGrpAttr.enDieMode = VPSS_DIE_MODE_NODIE;
    //stGrpAttr.enDieMode = VPSS_DIE_MODE_AUTO;
    //stGrpAttr.enDieMode = VPSS_DIE_MODE_DIE;
    stGrpAttr.enPixFmt = SAMPLE_PIXEL_FORMAT;
    s32Ret = SAMPLE_COMM_VPSS_Start(s32VpssGrpCnt, &stSize, VPSS_MAX_CHN_NUM,NULL);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start Vpss failed!\n");
        goto END_8X1080P_1;
    }

    s32Ret = SAMPLE_COMM_VI_BindVpss(enViMode);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Vi bind Vpss failed!\n");
        goto END_8X1080P_2;
    }
#endif

    /******************************************
     step 5: start vo HD0 (HDMI+VGA), multi-screen, you can switch mode
    ******************************************/
#if 1
    printf("start vo HD0.\n");
    VoDev = SAMPLE_VO_DEV_DHD0;
    VoLayer = SAMPLE_VO_LAYER_VHD0;
    u32WndNum = 8;
    enVoMode = VO_MODE_4MUX ;

    //stVoPubAttr.enIntfSync = VO_OUTPUT_1080P60;
    stVoPubAttr.enIntfSync = VO_OUTPUT_1080P50;
    stVoPubAttr.enIntfType = VO_INTF_HDMI|VO_INTF_VGA;
    stVoPubAttr.u32BgColor = 0x000000ff;
    s32Ret = SAMPLE_COMM_VO_StartDev(VoDev, &stVoPubAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start SAMPLE_COMM_VO_StartDev failed!\n");
        goto END_8X1080P_3;
    }

    memset(&(stLayerAttr), 0 , sizeof(VO_VIDEO_LAYER_ATTR_S));
    s32Ret = SAMPLE_COMM_VO_GetWH(stVoPubAttr.enIntfSync, \
        &stLayerAttr.stImageSize.u32Width, \
        &stLayerAttr.stImageSize.u32Height, \
        &stLayerAttr.u32DispFrmRt);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start SAMPLE_COMM_VO_GetWH failed!\n");
        goto END_8X1080P_3;
    }

    stLayerAttr.enPixFormat = SAMPLE_PIXEL_FORMAT;
    stLayerAttr.stDispRect.s32X 	  = 0;
    stLayerAttr.stDispRect.s32Y 	  = 0;
    stLayerAttr.stDispRect.u32Width   = stLayerAttr.stImageSize.u32Width;
    stLayerAttr.stDispRect.u32Height  = stLayerAttr.stImageSize.u32Height;
    s32Ret = SAMPLE_COMM_VO_StartLayer(VoLayer, &stLayerAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start SAMPLE_COMM_VO_StartLayer failed!\n");
        goto END_8X1080P_3;
    }


    s32Ret = SAMPLE_COMM_VO_StartChn(VoLayer, enVoMode);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start SAMPLE_COMM_VO_StartChn failed!\n");
        goto END_8X1080P_4;
    }

#ifdef HDMI_SUPPORT
    /* if it's displayed on HDMI, we should start HDMI */
    if (stVoPubAttr.enIntfType & VO_INTF_HDMI)
    {
        if (HI_SUCCESS != SAMPLE_COMM_VO_HdmiStart(stVoPubAttr.enIntfSync))
        {
            SAMPLE_PRT("Start SAMPLE_COMM_VO_HdmiStart failed!\n");
            goto END_8X1080P_4;
        }
    }
#endif

    for(i=0;i<2;i++)
    {
        VoChn = i;
        VpssGrp = i;

        s32Ret = SAMPLE_COMM_VO_BindVpss(VoDev,VoChn,VpssGrp,VpssChn_VoHD0);

#if 1 // ln debug
        //s32Ret = SAMPLE_COMM_VO_BindVpss(VoDev,VoChn,VpssGrp,VPSS_CHN3);
        //s32Ret = SAMPLE_COMM_VO_BindVpss(VoDev,VoChn,VpssGrp,VPSS_CHN3);

        //s32Ret = SAMPLE_COMM_VO_BindVi(0,VoChn,i * 4);
#endif
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("Start VO failed!\n");
            goto END_8X1080P_4;
        }
    }
#endif

    //-------------------------------------
#if 1 // ln debug add virtual vo dev
    printf("start vo virtual.\n");
    VoDev = SAMPLE_VO_DEV_VIRT0;
    VoLayer = SAMPLE_VO_LAYER_VIRT0;
    //
    u32WndNum = 2;
    //u32WndNum = 1;
    //enVoMode = VO_MODE_9MUX  ;
    enVoMode = VO_MODE_4MUX  ;

    //stVoPubAttr.enIntfSync = VO_OUTPUT_1080P30;
    stVoPubAttr.enIntfSync = VO_OUTPUT_1080P25;
    //
    stVoPubAttr.enIntfType = VO_INTF_HDMI;
    stVoPubAttr.u32BgColor = 0x000ff00;
    s32Ret = SAMPLE_COMM_VO_StartDev(VoDev, &stVoPubAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start SAMPLE_COMM_VO_StartDev failed!\n");
        goto END_8X1080P_3;
    }

    memset(&(stLayerAttr), 0 , sizeof(VO_VIDEO_LAYER_ATTR_S));

    s32Ret = SAMPLE_COMM_VO_GetWH(stVoPubAttr.enIntfSync, \
        &stLayerAttr.stImageSize.u32Width, \
        &stLayerAttr.stImageSize.u32Height, \
        &stLayerAttr.u32DispFrmRt);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start SAMPLE_COMM_VO_GetWH failed!\n");
        goto END_8X1080P_3;
    }

    stLayerAttr.enPixFormat = SAMPLE_PIXEL_FORMAT;
    //stLayerAttr.enPixFormat = PIXEL_FORMAT_YUV_SEMIPLANAR_422;
    stLayerAttr.stDispRect.s32X 	  = 0;
    stLayerAttr.stDispRect.s32Y 	  = 0;
    stLayerAttr.stDispRect.u32Width   = stLayerAttr.stImageSize.u32Width;
    stLayerAttr.stDispRect.u32Height  = stLayerAttr.stImageSize.u32Height;
    s32Ret = SAMPLE_COMM_VO_StartLayer(VoLayer, &stLayerAttr);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start SAMPLE_COMM_VO_StartLayer failed!\n");
        goto END_8X1080P_3;
    }

    //s32Ret = SAMPLE_COMM_VO_StartChn(VoLayer, enVoMode);
    s32Ret = SAMPLE_COMM_VO_StartChn_2(VoLayer);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start SAMPLE_COMM_VO_StartChn failed!\n");
        goto END_8X1080P_4;
    }
#if 0
    stGrpAttr.u32MaxW = 1920;
    stGrpAttr.u32MaxH = 1080;
    stGrpAttr.bNrEn = HI_TRUE;
    //stGrpAttr.bNrEn = HI_FALSE;
    stGrpAttr.enDieMode = VPSS_DIE_MODE_NODIE;
    //stGrpAttr.enDieMode = VPSS_DIE_MODE_AUTO;
    //stGrpAttr.enDieMode = VPSS_DIE_MODE_DIE;
    stGrpAttr.enPixFmt = SAMPLE_PIXEL_FORMAT;
    s32Ret = SAMPLE_COMM_VPSS_Start(s32VpssGrpCnt, &stSize, VPSS_MAX_CHN_NUM,NULL);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start Vpss failed!\n");
        goto END_8X1080P_1;
    }
#endif


    for(i=0;i<2;i++)
    {
        VoChn = i;
        VpssGrp = i;

#if 1 // ln debug
        s32Ret = SAMPLE_COMM_VO_BindVpss(3,VoChn,VpssGrp,VPSS_CHN3);
#else
        //s32Ret = SAMPLE_COMM_VO_BindVpss(3, VoChn, i, VPSS_CHN2);
        //virtual chn
        s32Ret = SAMPLE_COMM_VO_BindVi(3,VoChn,i * 2);
#endif
        if (HI_SUCCESS != s32Ret)
        {
            SAMPLE_PRT("Start VO failed!\n");
            goto END_8X1080P_4;
        }
    }
    /******************************************
      step 5: start stream venc (big + little)
     ******************************************/
    PAYLOAD_TYPE_E enPayLoad[3]= {PT_H264, PT_JPEG,PT_H264};
    PIC_SIZE_E enSize[3] = {PIC_HD1080, PIC_HD720,PIC_CIF};
    //int enRcMode = SAMPLE_RC_VBR;
    int enRcMode = SAMPLE_RC_CBR;
    HI_U32 u32Profile = 1; /*0: baseline; 1:MP; 2:HP 3:svc-t */

    i = 0;
    /*** main stream,H264**/
    int VencChn = i*2;
    VpssGrp = i;

    VIDEO_NORM_E gs_enNorm = VIDEO_ENCODING_MODE_NTSC;

    s32Ret = SAMPLE_COMM_VENC_Start(VencChn, enPayLoad[0], gs_enNorm, enSize[0], enRcMode,u32Profile);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start Venc failed!\n");
        exit(-1);
    }
    //s32Ret = SAMPLE_COMM_VENC_BindVpss(VencChn,VpssGrp, VPSS_BSTR_CHN);

    s32Ret = SAMPLE_COMM_VENC_BindVo(3, 0,0);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("SAMPLE_COMM_VENC_BindVo failed!\n");
        exit(-1);
    }

    /******************************************
     step 6: stream venc process -- get stream, then save it to file.
    ******************************************/
    s32Ret = SAMPLE_COMM_VENC_StartGetStream(1);
    if (HI_SUCCESS != s32Ret)
    {
        SAMPLE_PRT("Start Venc failed!\n");
        exit(-1);
    }


#endif


    /******************************************
    step 8: HD0 switch mode
    ******************************************/
    VoDev = SAMPLE_VO_DEV_DHD0;
    VoLayer = SAMPLE_VO_LAYER_VHD0;
    enVoMode = VO_MODE_9MUX;
    while(1)
    {
        return 0;
    }

    /******************************************
     step 8: exit process
    ******************************************/


END_8X1080P_5:
    VoDev = SAMPLE_VO_DEV_DSD0;
    VoLayer = SAMPLE_VO_LAYER_VSD0;
    VoChn = 0;
    enVoMode = VO_MODE_9MUX;
    SAMPLE_COMM_VO_StopChn(VoLayer, enVoMode);
    SAMPLE_COMM_VO_StopLayer(VoLayer);
    SAMPLE_COMM_VO_StopDev(VoDev);

END_8X1080P_4:
#ifdef HDMI_SUPPORT
    if (stVoPubAttr.enIntfType & VO_INTF_HDMI)
    {
        SAMPLE_COMM_VO_HdmiStop();
    }
#endif
    VoDev = SAMPLE_VO_DEV_DHD0;
    VoLayer = SAMPLE_VO_LAYER_VHD0;
    u32WndNum = 8;
    enVoMode = VO_MODE_9MUX;
    SAMPLE_COMM_VO_StopChn(VoDev, enVoMode);
    for(i=0;i<u32WndNum;i++)
    {
        VoChn = i;
        VpssGrp = i;
        SAMPLE_COMM_VO_UnBindVpss(VoDev,VoChn,VpssGrp,VpssChn_VoHD0);
    }
    SAMPLE_COMM_VO_StopLayer(VoLayer);
    SAMPLE_COMM_VO_StopDev(VoDev);

END_8X1080P_3:
    SAMPLE_COMM_VI_UnBindVpss(enViMode);
END_8X1080P_2:
    SAMPLE_COMM_VPSS_Stop(s32VpssGrpCnt, VPSS_MAX_CHN_NUM);
END_8X1080P_1:
    SAMPLE_COMM_VI_Stop(enViMode);
END_8X1080P_0:
    SAMPLE_COMM_SYS_Exit();

    return s32Ret;
}

#if 0
/******************************************************************************
* function    : main()
* Description : video preview sample
******************************************************************************/
int main(int argc, char *argv[])
{
    int s32Ret;

    rtmp_h264_server_start();
    s32Ret = SAMPLE_VIO_8_1080P_DUAL();

    exit(s32Ret);
}
#endif

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */

