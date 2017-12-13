#ifndef __CONTAINER__
#define __CONTAINER__


#include <libavformat/avformat.h>
#include <libavutil/avstring.h>
#include <libavutil/dict.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libavutil/avassert.h>

#include <stdbool.h>
#include <stdint.h>
#include <hi_comm_venc.h>
#include <hi_comm_aio.h>
#include "streamblock.h"

#if __cplusplus
extern "C"{
#endif
//
#define MAX_FILE_NAME_LEN 1024

int get_raw_stream_v(int chn, Mal_StreamBlock *block, void* opaque);
int get_raw_stream_a(int chn, Mal_StreamBlock *block, void* opaque);

int container_send_audio( Mal_StreamBlock *block);
int container_send_video( Mal_StreamBlock *block);

typedef void (*get_ripe_stream_fun)(struct ripe_stream_s *stream);
struct global_param_s{
    get_ripe_stream_fun fun;
    struct chn_param_s *chn_params;
};

//send ripe mp4 data interface
struct ripe_stream_s{
    void *data;
};

//get and set param of container
struct container_param_s{
    bool audio_enable;

    char file_path[MAX_FILE_NAME_LEN];
    int last_time;

    SIZE_S video_size;
    int video_frame_rate;

    int audio_sample_rate;
    int audio_bit_width;
    int audio_sound_mode;
    int audio_num_pre_frame;
};

//muxer
struct chn_param_s{
    int chn_num;
    bool video_enable;
    bool audio_enable;
    bool audio_write_ready;
    bool is_writing_file;
    char file_dir[MAX_FILE_NAME_LEN];
    char file_name[MAX_FILE_NAME_LEN];
    uint64_t  last_time;

    SIZE_S video_size;
    int video_frame_rate;

    int audio_sample_rate;
    int audio_bit_width;
    int audio_sound_mode;
    int audio_num_pre_frame;

    AVFormatContext *out_context;
    AVStream *video_stream;
    AVStream *audio_stream;

    uint64_t first_packet_pts;
    uint64_t last_packet_pts;

    int64_t register_size_bound;

    pthread_mutex_t mutex;
};


int get_container_param(int chn, struct container_param_s *param);
int set_container_param(int chn, struct container_param_s *param);


int container_start_new_file(char *file_name, int chn);

//async
int switch_new_file(int chn, struct container_param_s *param);
int stop_container(int chn);

//construct and destory
int container_init(get_ripe_stream_fun callback);
int container_destory();

#if __cplusplus
}
#endif
#endif

