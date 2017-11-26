#include <stdbool.h>
int get_save_path(char *path)
{

}

int status_get_venc_audio_enable(int chn)
{
    //return chn_ini_get_int(chn, "audio_enable");
}

int status_get_chn_venc_file_last_time(int chn)
{
    //return chn_ini_get_int(chn, "last_time");
}

int status_get_chn_venc_frame_rate(int chn)
{
    //return chn_ini_get_int(chn, "venc_frame_rate");
}

int watch_get_actual_use_chn()
{
    return 0;
}
int set_is_there_kinescope(bool is_there_kinescope)
{
#if 0
    int ret;
    struct trans_video_setting_st *info;
    ret = setting_trans_get(&info, 2000);
    if(ret != 0){
        err_msg("setting_trans_get failed\n");
    }
    info->vc_over.current_kinescope_status = is_there_kinescope;
    setting_trans_get_release(&info);
#endif
    return 0;
}

int should_change_storage_disk()
{
#if 0
    int disk_index;
    int change_disk_index;
    int ret;

    //1.检查是否存在脏数据
    ret = is_there_dirty_file();
    if(ret != 0){
        return 0;
    }

    //2.取得当前的硬盘索引，查看需要切换的硬盘是否存在，如果存在就切换
    disk_index = watch_disk_status.current_disk_index;
    change_disk_index = 1 - disk_index;
    info_msg("================current disk_index = %d, change_disk_index = %d\n", disk_index, change_disk_index);

    //1.已经确定硬件是存在的，开始切换存储设备
    if(watch_disk_status.storage[change_disk_index].is_disk_exist){
        info_msg("==================begin to change disk to %d===================\n", change_disk_index);
        //2.首先改变status中的存储位置
        watch_disk_status.current_disk_index = change_disk_index;
        status_set_current_store_index(watch_disk_status.current_disk_index);

        //3.改变存储chn_save的标识位
        //info_msg("before pthread_mutex_lock\n");
        pthread_mutex_lock(&venc_chn_save.mutex);
        int i;
        for(i = 0; i < MAX_VENC_CHN_SAVE; i++){
            if(venc_chn_save.chn_save[i] == SAVE_FILE_NORMAL){
                //info_msg("change venc_chn_save.chn_save\n");
                venc_chn_save.chn_save[i] = SAVE_FILE_CHANGE_STORAGE;
            }
        }
        venc_chn_save.is_sqlite_ok = false;

        //4.关闭先前的数据库，更换数据库,等待container 写文件尾
        info_msg("before pthread_cond_wait\n");

        pthread_cond_wait(&venc_chn_save.cond, &venc_chn_save.mutex);
        info_msg("after pthread_cond_wait\n");
        //4.1
        close_pre_and_open_current_db();
        //4.2删除所有先前的文件, 标记所有文件为脏
        char sql_command[SQL_CLAUSE_LEN] = {0};

        for(i = 0; i < 8; i++){
            memset(sql_command, 0, sizeof(sql_command));
            sprintf(sql_command, "update chn%d set is_dirty = 1;", i);
            char *mess = NULL;
            ret = sqlite3_exec(db_status.db, sql_command, NULL, NULL, &mess);
            if(ret != SQLITE_OK){
                err_msg("update dirty data failed. %s\n", mess);
                sqlite3_free(mess);
                return -1;
            }
        }

        venc_chn_save.is_sqlite_ok = true;
        pthread_mutex_unlock(&venc_chn_save.mutex);

        return 1;
    }else{
        return 0;
    }
#endif
}

int update_setting_trans_disk_status()
{
#if 0
    struct trans_video_setting_st *info;
    int ret;

    struct disk_used_disk{
        int pic;
        int video;
    }disk[2];

    memset(disk, 0, sizeof(disk));
    int i;

    for(i = 0; i < 2; i++){
        if(watch_disk_status.storage[i].is_disk_exist){
            disk[i].video = get_video_used_size(db_status.db, &db_status.mutex);
            disk[i].pic = get_picture_used_size(db_status.db,&db_status.mutex);
        }
    }

    ret = setting_trans_get(&info, 3000);
    if(ret != 0){
        err_msg("setting_trans_get failed\n");
        return -1;
    }
    for(i = 0; i < 2; i++){
        struct storage_st *storage = &watch_disk_status.storage[i];
        info->hard_disk_setting.hard_disk[i].total_capability =storage->picture_capability +storage->video_capability;
        info->hard_disk_setting.hard_disk[i].picture_partition_size =storage->picture_capability;
        info->hard_disk_setting.hard_disk[i].video_partition_size =storage->video_capability;

        info->hard_disk_setting.hard_disk[i].used_picture_size = disk[i].pic;
        info->hard_disk_setting.hard_disk[i].used_video_size = disk[i].video;

        //info_msg("disk = %d, picture size = %dKB,video_size = %dKB, used_picture_size = %dKB, used_video_size = %dKB\n", i, info->hard_disk_setting.hard_disk[i].picture_partition_size, info->hard_disk_setting.hard_disk[i].video_partition_size, info->hard_disk_setting.hard_disk[i].used_picture_size, info->hard_disk_setting.hard_disk[i].used_video_size);
    }

    setting_trans_get_release(&info);
    return 0;
#endif
}

int status_get_current_store_media()
{
    //return over_ini_get_int("current_store_media");
}

int status_get_current_store_index()
{
    //return over_ini_get_int("current_store_index");
}

int status_set_current_store_index(int store_index)
{
    //over_ini_set_int("current_store_index", store_index);
    //status_sync_mem_to_ini();
    return 0;
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

