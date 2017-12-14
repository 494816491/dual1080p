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
            //info_msg("before %d callback", thread_info->thread);
            thread_info->callback(thread_info->block);
            //info_msg("after %d callback", thread_info->thread);
        }

        //get mutex to clear work_bit
        pthread_mutex_lock(&stream_cond->mutex);
        //info_msg("before stream_cond->work_status = %d\n", stream_cond->work_status);
        stream_cond->work_status = stream_cond->work_status & ~(0x1 << thread_info->work_status_bit);
        //info_msg("after stream_cond->work_status = %d\n", stream_cond->work_status);

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
            container_send_video,
};

    //for(i = 0; i < 4; i++){
    for(i = 1; i < 2; i++){
        struct distri_thread_info_st *thread_info  = thread_infos[i];
        int (*callback)(Mal_StreamBlock *block) = callbacks[i];
        thread_info->callback = callback;
        pthread_create(&thread_info->thread, NULL, distribute_stream_thread_func, thread_info);
    }

    for(i = 3; i < 4; i++){
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
    param->audio_enable = 0;
    param->audio_sample_rate = 8000;

    memset(param->file_path, 0, sizeof(param->file_path));

    //必须的加上/mnt/video/,否则的话container有问题
    sprintf(param->file_path, VIDEO_SAVE_PATH);

    param->last_time = 300;

    param->video_size.u32Width = 1920;
    param->video_size.u32Height = 1080;

    param->video_frame_rate = 25;

    return 0;
}

int start_watch_routine()
{
    int i;
    int ret;


    struct rtmp_chn_param_st param = {0};
    param.audio_enable = 0;
    param.chn_num = 0;
#if 1
    sprintf(param.ip_addr, "rtmp://192.168.22.2/live/chn0");
#else
    sprintf(param.ip_addr, "rtmp://ps3.live.panda.tv/live_panda/fdd3dac64c9f2df18898a695b1b2bfa5?sign=6cb56c29a6924d6ac20790c91b4ee06d&time=1512825939&wm=2&wml=1&vr=6&extra=0");
#endif

    param.video_frame_rate = 25;
    param.video_w = 1920;
    param.video_h = 1080;
    set_rtmp_chn_param(0, &param);
    open_rtmp_stream(0);


    for(i = 0; i < 1; i++){
        struct container_param_s param = {0};
        watch_get_container_save_param(i, &param);
        set_container_param(i, &param);
    }

    //开始时钟
    uint32_t time_slice = 0;
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    int last_sec = tp.tv_sec;

    disk_manage_init();
    container_start_new_file("/mnt/usb/test.mkv", 0);

    while(1){
#if 1
        time_slice++;
        if(is_disk_is_exit()){
            if((time_slice % 60) == 59){
                //删除之前的数据
                while(should_delete_old_file()){
                    ret = delete_pre_video_file();
                    if(ret != 0){
                        err_msg("rm_pre_file failed\n");
                    }
                }
            }

            //是否需要切换新的文件
            struct timespec current_tp;
            clock_gettime(CLOCK_MONOTONIC, &current_tp);

            if(current_tp.tv_sec - last_sec > 60){
                last_sec = current_tp.tv_sec;
                info_msg("switch_new_file");
                switch_new_file(0, NULL);
            }
            if(current_tp.tv_sec - tp.tv_sec  > 5 ){
                tp = current_tp;

            }
        }else{
            info_msg("disk is not exist");
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

        if(current_tp.tv_sec - tp.tv_sec  > 5 ){
            tp = current_tp;

        }

        tp.tv_sec++;
        ret = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &tp, NULL);
        if(ret != 0){
            info_msg("clock_nanosleep failed\n");
        }
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
