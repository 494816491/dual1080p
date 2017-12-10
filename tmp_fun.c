#include <stdbool.h>




int watch_get_actual_use_chn()
{
    return 0;
}

int status_get_current_store_media()
{
    //return over_ini_get_int("current_store_media");
}

int status_get_current_store_index()
{
    //return over_ini_get_int("current_store_index");
}


typedef enum vi_mode_e
{   /* For Hi3531 or Hi3532 */
    STATUS_VI_MODE_PAL = 0,
    STATUS_VI_MODE_NTSC ,
    STATUS_VI_MODE_720P ,
}STATUS_VI_MODE_E;

STATUS_VI_MODE_E status_get_vi_mode()
{
    //return over_ini_get_int("vi_mode");
}
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

