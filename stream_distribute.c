#include "stream_distribute.h"
#include "container.h"
#include "sqlite3.h"
#include "disk_manage.h"
#include "debug.h"

#define MAX_VENC_CHN_SAVE 8

enum SAVE_FILE_CONTROL{
    SAVE_FILE_STOP = 0,
    SAVE_FILE_NORMAL,
    SAVE_FILE_START,
    SAVE_FILE_SUSPEND,
    SAVE_FILE_CHANGE_STORAGE
};

static struct venc_chn_save_st{
    int actual_use_chn;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool is_sqlite_ok;
    int chn_save[MAX_VENC_CHN_SAVE];
    uint32_t chn_register_count[MAX_VENC_CHN_SAVE];
}venc_chn_save;




struct db_status_st{
    sqlite3 *db;
    pthread_mutex_t mutex;
};

static struct db_status_st db_status = { NULL, PTHREAD_MUTEX_INITIALIZER};



void translate_venc_stream(int channel,  VENC_STREAM_S *pstStream)
{
    int i=0;
    Mal_StreamBlock block;
    //info_msg("block.chn_num = %d\n", channel);

    if(channel >= 2)
        return;

    for (i = 0; i < pstStream->u32PackCount; i++) {
        memset(&block, 0, sizeof(block));
        block.chn_num = channel;
        block.i_buffer = pstStream->pstPack[i].u32Len;
        block.p_buffer = malloc(block.i_buffer);
        if(block.p_buffer == 0){
            err_msg("malloc failed\n");
            return;
        }
        //copy one packet
        memcpy(block.p_buffer, pstStream->pstPack[i].pu8Addr, pstStream->pstPack[i].u32Len);


        //block.i_flags = convertFlags(pstStream->pstPack[i].DataType.enH264EType);
        block.i_flags = pstStream->pstPack[i].DataType.enH264EType;
        block.i_codec = AV_FOURCC_H264;
        block.i_pts = pstStream->pstPack[i].u64PTS;

        //s->callbacks[channel].cb(channel, &block, s->callbacks[channel].opaque);
        //ln add
        send_rtmp_video_stream(&block);

        free(block.p_buffer);
    }
}

void translate_audio_stream(int channel, VENC_STREAM_S *pstStream)
{

}

int watch_get_container_save_param(int chn, struct container_param_s *param)
{
    param->audio_enable = status_get_venc_audio_enable(chn);
    param->audio_sample_rate = 8000;

    memset(param->file_path, 0, sizeof(param->file_path));

    //必须的加上/mnt/video/,否则的话container有问题
    //sprintf(param->file_path, "/mnt/video%d", watch_disk_status.current_disk_index);

    get_save_path(param->file_path);

    param->last_time = status_get_chn_venc_file_last_time(chn);

    param->video_size.u32Width = 1920;
    param->video_size.u32Height = 1080;

    param->video_frame_rate = status_get_chn_venc_frame_rate(chn);
    return 0;
}

int distribute_stream()
{
    int i;
    struct container_param_s param = {0};
    watch_get_container_save_param(i, &param);
    set_container_param(i, &param);
}


int start_watch_routine()
{
    int i;
    int ret;
    //judge is sd or hard disk
    initialize_watch_disks_status();

    //初始化数据库
    if(watch_disk_status.is_storage_exist){
        if(db_status.db == NULL){
            if(open_init_db() != 0){
                err_msg("open_init_db failed\n");
                return -1;
            }
            //查找数据库中没有记录大小的文件，添加大小记录,开机做一次检查
            check_database_size();
            info_msg("after check_database_size\n");
        }

        for(i = 0; i < watch_get_actual_use_chn(); i++){
            venc_control_param_to_monitor(i);
        }
    }

    //info_msg("after change disk, is_storage_disk_exist = %d\n", watch_disk_status.is_storage_exist);

    //start to get video and audio data
    //set container param
#if 1
    for(i = 0; i < 8; i++){
        struct container_param_s param = {0};
        watch_get_container_save_param(i, &param);
        set_container_param(i, &param);
    }
    //10.挂接获取视频流

#if MAL_VI_INPUT_VIDEO
    for(i = 0; i < watch_get_actual_use_chn(); i++){
        stream_add_callback(i, &streamCallback, NULL);
    }
    for(i = 8; i < 8 + watch_get_actual_use_chn(); i++){
        stream_add_callback(i, &streamCallback, NULL);
    }
#endif
    //11.挂接获取音频流
#if MAL_AUDIO_COMPRESS_SERVICE
    for(i = 0; i < 8; i++){
        audio_add_callback(i, &audioStreamCallback, NULL);
    }
#endif
#endif

    //开始时钟
    uint32_t time_slice = 0;
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
#define WHILE_DEBUG 0

    while(1){

        //info_msg("while \n");
#if 1
        time_slice++;

        //info_msg("while1 \n");
        if(watch_disk_status.is_storage_exist){
            //info_msg("while2 \n");
#if 1
            if((time_slice & 0b11) == 0){
                //判断更新最新的录像控制状态
                for(i = 0; i < watch_get_actual_use_chn(); i++){
                    venc_control_param_to_monitor(i);
                    //info_msg("venc_control_param_to_monitor---------------\n");
                }
                //判断当前是否有通道在写硬盘,并且将状态写入共享内存中
                bool is_there_kinescope = 0;
                for(i = 0; i < watch_get_actual_use_chn(); i++){
                    is_there_kinescope = is_there_kinescope || venc_chn_save.chn_save[i];
                }

                set_is_there_kinescope(is_there_kinescope);

            }
            if((time_slice & 0b111111) == 0b11){
                int free_disk;
                //info_msg("before get_free_video_disk\n");
                free_disk = get_free_video_disk();
                //info_msg("after get_free_video_disk\n");

                //info_msg("watch_disk_status.storage[watch_disk_status.current_disk_index].remain_video_disk = %d\n", watch_disk_status.storage[watch_disk_status.current_disk_index].remain_video_disk);
                while(free_disk <= watch_disk_status.storage[watch_disk_status.current_disk_index].remain_video_disk){
                    //判断是否应该切换硬盘
                    if(!should_change_storage_disk()){
                        ret = delete_pre_video_file(db_status.db, &db_status.mutex);
                        if(ret != 0){
                            err_msg("rm_pre_file failed\n");
                        }
                    }
                    free_disk = get_free_video_disk();
                }
            }

            if((time_slice & 0b11111) == 0b1111){ //info_msg("----------------update_setting_trans_disk_status-------------\n");
                update_setting_trans_disk_status();
                //info_msg("after update_setting_trans_disk_status\n");
            }

#endif
        }
#if MAL_VI_INPUT_VIDEO
        //info_msg("while7 \n");
        draw_osd_1s();
        vi_loss_detection_1s();
#endif

#endif
        //修正时间，防止时间跨越
        struct timespec current_tp;
        //info_msg("while8 \n");
        clock_gettime(CLOCK_MONOTONIC, &current_tp);
        //printf("current time = %ld, tp time = %ld\n", (long)current_tp.tv_sec, (long)tp.tv_sec);
        if(current_tp.tv_sec - tp.tv_sec  > 5 ){
            tp = current_tp;
        }
        //info_msg("while9 \n");
        tp.tv_sec++;
        ret = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &tp, NULL);
        //info_msg("while10 \n");
        if(ret != 0){
            info_msg("clock_nanosleep failed\n");
        }
        //info_msg("clock_nanosleep\n");
    }

    //回收资源
    info_msg("container stop\n");

    for(i = 0; i < watch_get_actual_use_chn(); i++){
        venc_chn_save.chn_save[i] = SAVE_FILE_STOP;
        //info_msg("venc_control_param_to_monitor---------------\n");
        stop_container(i);
    }
    info_msg("stop_cmd_loop\n");
    stop_cmd_loop();
    info_msg("Mal_SYS_Exit\n");
    Mal_SYS_Exit();
    info_msg("setting_trans_destory\n");
    setting_trans_destory();
    info_msg("status_sync_mem_to_ini\n");
    status_sync_mem_to_ini();
    info_msg("clog_exit\n");
    exit(0);

    return 0;
}
