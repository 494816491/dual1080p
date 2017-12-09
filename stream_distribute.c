#include "stream_distribute.h"
#include "rtmp_h264.h"
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

enum STREAM_SINK_ENUM{
    RTMP_SINK = 0,
    MKV_SINK,
    BUTT_SINK,
};

struct distri_thread_info_st{
    //
    pthread_t thread;
    struct stream_cond_st *stream_cond;
    int work_status_bit;
    //
    int (*callback)(Mal_StreamBlock *block);
    Mal_StreamBlock *block;
};

struct stream_cond_st{
    pthread_mutex_t mutex;
    pthread_cond_t work_cond;//start to work or not, 0. not work
    //pthread_cond_t complete_cond;
    struct distri_thread_info_st thread_info[BUTT_SINK];

    int work_status;
};

static struct stream_distri_st{
    //struct stream_sink_st sink_info[BUTT_SINK];
    struct stream_cond_st a_stream_cond;
    struct stream_cond_st v_stream_cond;
}stream_distri_info;

static int init_stream_cond(struct stream_cond_st *stream_cond)
{
    int i;
    pthread_mutex_init(&stream_cond->mutex, NULL);
    pthread_cond_init(&stream_cond->work_cond, NULL);
    for(i = 0; i < BUTT_SINK; i++){
        struct distri_thread_info_st *thread_info = &stream_cond->thread_info[i];
        thread_info->stream_cond = stream_cond;
        thread_info->work_status_bit = i;
    }

    return 0;
}

//thread only do one thing, get data, call callback
void *distribute_stream_thread_func(void *data)
{
    struct distri_thread_info_st *thread_info = (struct distri_thread_info_st *)data;
    struct stream_cond_st *stream_cond = thread_info->stream_cond;
    while(1){
        //check is there work to do, then wait
        while(!(stream_cond->work_status & (0x1 << thread_info->work_status_bit))){
            pthread_cond_wait(&stream_cond->work_cond, &stream_cond->mutex);
        }

        //release mutext to let other thread work
        pthread_mutex_unlock(&stream_cond->mutex);

        //work
        if(thread_info->callback){
            info_msg("before %d callback", thread_info->thread);
            thread_info->callback(thread_info->block);
            info_msg("after %d callback", thread_info->thread);
        }

        //get mutex to clear work_bit
        pthread_mutex_lock(&stream_cond->mutex);
        info_msg("before stream_cond->work_status = %d\n", stream_cond->work_status);
        stream_cond->work_status = stream_cond->work_status & ~(0x1 << thread_info->work_status_bit);
        info_msg("after stream_cond->work_status = %d\n", stream_cond->work_status);

        //say work is done, release mutex
        pthread_cond_broadcast(&stream_cond->work_cond);
    }
    return (void *)0;
}

int stream_distri_init()
{
    //
    int i;
    //init stream cond,and thread
    init_stream_cond(&stream_distri_info.a_stream_cond);
    init_stream_cond(&stream_distri_info.v_stream_cond);

    //hook thread callbacks;
    struct distri_thread_info_st *thread_infos[4] = {
                &stream_distri_info.a_stream_cond.thread_info[RTMP_SINK],
                &stream_distri_info.v_stream_cond.thread_info[RTMP_SINK],
                &stream_distri_info.a_stream_cond.thread_info[MKV_SINK],
                &stream_distri_info.v_stream_cond.thread_info[MKV_SINK]
                };

    int (*callbacks[4])(Mal_StreamBlock *block) = {
            send_rtmp_audio_stream,
            send_rtmp_video_stream,
            container_send_audio,
            container_send_audio
};

    //for(i = 0; i < 4; i++){
    for(i = 1; i < 2; i++){
        struct distri_thread_info_st *thread_info  = thread_infos[i];
        int (*callback)(Mal_StreamBlock *block) = callbacks[i];
        thread_info->callback = callback;
        pthread_create(&thread_info->thread, NULL, distribute_stream_thread_func, thread_info);
    }

    return 0;
}

static int distribute_stream(struct stream_cond_st *stream_cond, Mal_StreamBlock *block)
{
    int i;
    pthread_mutex_lock(&stream_cond->mutex);
    int has_work;

    //distribute streams to different threads
    for(i = 0; i < BUTT_SINK; i++){
        stream_cond->thread_info[i].block = block;
        if(stream_cond->thread_info[i].callback){
            stream_cond->work_status |= (0x1 << stream_cond->thread_info[i].work_status_bit);
            has_work = 1;
        }
    }

    pthread_cond_broadcast(&stream_cond->work_cond);

    //if has work wait to be aweak, or just go on
    if(has_work){
        while(stream_cond->work_status){
            pthread_cond_wait(&stream_cond->work_cond, &stream_cond->mutex);
        }
    }

    pthread_mutex_unlock(&stream_cond->mutex);

    return 0;
}

void translate_venc_stream(int channel,  VENC_STREAM_S *pstStream)
{
    int i=0;
    Mal_StreamBlock block;
    info_msg("block.chn_num = %d\n", channel);

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
#if 1
        distribute_stream(&stream_distri_info.v_stream_cond, &block);
#endif

        free(block.p_buffer);
    }
}

void translate_audio_stream(int channel, AUDIO_STREAM_S *pstStream)
{
    Mal_StreamBlock block;
    block.chn_num  = channel;

    memset(&block, 0, sizeof(block));
    block.i_buffer = pstStream->u32Len - 4;
    block.p_buffer = (char *)pstStream->pStream + 4;
    block.i_pts = pstStream->u64TimeStamp;
    block.i_codec = AV_FOURCC_G711A;
#if 1
    distribute_stream(&stream_distri_info.a_stream_cond, &block);
#endif
    return 0;
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

int start_watch_routine()
{
    int i;
    int ret;


    struct rtmp_chn_param_st param = {0};
    param.audio_enable = 0;
    param.chn_num = 0;
    //sprintf(param.ip_addr, "rtmp://192.168.22.2/live/chn0");
    sprintf(param.ip_addr, "rtmp://ps3.live.panda.tv/live_panda/fdd3dac64c9f2df18898a695b1b2bfa5?sign=6cb56c29a6924d6ac20790c91b4ee06d&time=1512825939&wm=2&wml=1&vr=6&extra=0");

    param.video_frame_rate = 25;
    param.video_w = 1920;
    param.video_h = 1080;
    set_rtmp_chn_param(0, &param);
    open_rtmp_stream(0);

    while(1){

    }

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
