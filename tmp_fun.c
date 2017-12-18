#include <stdbool.h>




typedef enum vi_mode_e
{   /* For Hi3531 or Hi3532 */
    STATUS_VI_MODE_PAL = 0,
    STATUS_VI_MODE_NTSC ,
    STATUS_VI_MODE_720P ,
}STATUS_VI_MODE_E;

#include "hi_common.h"

typedef HI_U32 RGN_HANDLE;
HI_S32 HI_MPI_RGN_DetachFrmChn(RGN_HANDLE Handle,const MPP_CHN_S *pstChn)
{

}

int status_get_venc_osd(int chn, char *osd, int length)
{
    //strncpy(osd, chn_ini_get_string(chn, "osd"), length);
    return 0;
}

