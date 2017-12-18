#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "container.h"
#include "debug.h"

#define REGISTER_INDEX 1
#define REGISTER_FILE_SIZE_BOUND 120000
//#define TS_TEST


static struct global_param_s *global_param;


int container_init(get_ripe_stream_fun callback)
{
    //1.alloc global_param space
    global_param = malloc(sizeof(struct global_param_s));
    if(global_param == NULL){
        err_msg("malloc failed");
        return -1;
    }
    memset(global_param, 0, sizeof(struct global_param_s));
    //alloc space of chn_params
    global_param->chn_params = malloc(sizeof(struct chn_param_s) * 8);
    if(global_param->chn_params == NULL){
        err_msg("malloc failed");
        return -1;
    }
    memset(global_param->chn_params, 0, sizeof(struct chn_param_s) * 8);

    //initilaze mutex
    int i;
    for(i = 0; i < 8; i++){
        pthread_mutex_init(&global_param->chn_params[i].mutex,NULL);
        global_param->chn_params[i].chn_num = i;
    }

    //2.hook callback fun
    global_param->fun = callback;

    //3.init ffmpeg
    av_register_all();
    avformat_network_init();

    return 0;
}

static inline int param_copy(struct chn_param_s *chn_param, struct container_param_s *param)
{
    chn_param->last_time = param->last_time;
    //convert file_name
    strncpy(chn_param->file_dir, param->file_path, sizeof(chn_param->file_name));

    chn_param->video_size.u32Height = param->video_size.u32Height;
    chn_param->video_size.u32Width = param->video_size.u32Width;
    chn_param->video_frame_rate = param->video_frame_rate;

    chn_param->audio_enable = param->audio_enable;
    chn_param->audio_sample_rate = param->audio_sample_rate;
    chn_param->audio_num_pre_frame = param->audio_num_pre_frame;

    return 0;
}
int set_container_param(int chn, struct container_param_s *param)
{
    if(chn < 0 || chn >=8){
        err_msg("chn num is wrong");
        return -1;
    }
    //calculate chn_param position
    struct chn_param_s *chn_param = global_param->chn_params + chn;

    pthread_mutex_lock(&chn_param->mutex);

    param_copy(chn_param, param);

    pthread_mutex_unlock(&chn_param->mutex);

    return 0;
}

int get_container_param(int chn, struct container_param_s *param)
{
    struct chn_param_s *chn_param = global_param->chn_params + chn;
    pthread_mutex_lock(&chn_param->mutex);

    param->last_time = chn_param->last_time;
    strncpy(param->file_path, chn_param->file_dir, sizeof(param->file_path));

    param->video_size.u32Height = chn_param->video_size.u32Height;
    param->video_size.u32Width = chn_param->video_size.u32Width;
    param->video_frame_rate = chn_param->video_frame_rate;

    param->audio_enable = chn_param->audio_enable;
    param->audio_sample_rate = chn_param->audio_sample_rate;
    param->audio_num_pre_frame = chn_param->audio_num_pre_frame;

    pthread_mutex_unlock(&chn_param->mutex);

    return 0;
}

static int add_video_stream(struct chn_param_s *chn_param)
{
    AVCodecContext *codec_context;

    chn_param->video_stream= avformat_new_stream(chn_param->out_context, NULL);
    if(chn_param->video_stream == NULL){
        err_msg("avformat_new_stream error\n");
        return -1;
    }
    codec_context = chn_param->video_stream->codec;
    //timestamp time_base
    chn_param->video_stream->time_base.num = 1;
    chn_param->video_stream->time_base.den = chn_param->video_frame_rate;

    //Mandatory fields as specified in AVCodecContext documentation must be set even if this AVCodecContext is not actually used for encoding
    codec_context = chn_param->video_stream->codec;
    //codec_context->codec_id = codec_id;
    codec_context->codec_id = AV_CODEC_ID_H264;
    codec_context->codec_type = AVMEDIA_TYPE_VIDEO;

    //must set width and height
    codec_context->width = chn_param->video_size.u32Width;
    codec_context->height = chn_param->video_size.u32Height;
    codec_context->time_base.num = 1;
    codec_context->time_base.den = chn_param->video_frame_rate;
    codec_context->gop_size = chn_param->video_frame_rate;

    // some formats want stream headers to be separate
    if(chn_param->out_context->oformat->flags & AVFMT_GLOBALHEADER){
        codec_context->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }

    return 0;
}

static int add_audio_stream(struct chn_param_s *chn_param)
{
    //int ret;
    AVStream *audio_stream;
    AVCodecContext *codec_context;
#if 1
    chn_param->audio_stream = avformat_new_stream(chn_param->out_context, NULL);
    if(chn_param->audio_stream == NULL){
        err_msg( "avformat_new_stream error,audio_stream = %p\n", chn_param->audio_stream);
        return -1;
    }
#endif
    audio_stream = chn_param->audio_stream;
#if 1
    //printf("before set codec par\n");
    codec_context = audio_stream->codec;
    //AV_CODEC_ID_PCM_ALAW

    //codec_context->bits_per_coded_sample = AV_SAMPLE_FMT_S16;

    codec_context->codec_id = AV_CODEC_ID_PCM_ALAW;
    codec_context->codec_type = AVMEDIA_TYPE_AUDIO;

    codec_context->sample_rate = chn_param->audio_sample_rate;
    codec_context->channels = 1;
    //codec_context->sample_fmt = AV_SAMPLE_FMT_S16;
    codec_context->bits_per_coded_sample = 8;
    //codec_context->bit_rate = 64000;
    codec_context->frame_size = 320;
    codec_context->time_base.den = 25;
    codec_context->time_base.num = 1;

    /* time base: this is the fundamental unit of time (in seconds) in terms
       timebase should be 1/framerate and timestamp increments should be
       identically 1. */
    audio_stream->time_base.num = 1;
    audio_stream->time_base.den = 25;
    // some formats want stream headers to be separate
    if(chn_param->out_context->oformat->flags & AVFMT_GLOBALHEADER){
        codec_context->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }
    //printf("after set codec par\n");
#endif
    return 0;
}


static int create_new_file(struct chn_param_s *chn_param)
{
    int ret;

    //av_log_set_level(AV_LOG_DEBUG);

    char file_time[MAX_FILE_NAME_LEN] = {0};
    char file_pre_dir[MAX_FILE_NAME_LEN] = {0};
    time_t get_time = time(NULL);
    //strftime(file_time, MAX_FILE_NAME_LEN, "%Y-%m-%d-%H-%M-%S", gmtime(&get_time));
    struct tm disperse_time;
    localtime_r(&get_time, &disperse_time);

    //is dir exist , if not mkdir it
    sprintf(file_pre_dir, "%s/%04dyear/%02dmonth/%02dday", chn_param->file_dir, disperse_time.tm_year + 1900, disperse_time.tm_mon + 1, disperse_time.tm_mday);
    ret = access(file_pre_dir, F_OK);
    if(ret != 0){
        char shell_cmd[200] = {0};
        sprintf(shell_cmd, "mkdir -p %s", file_pre_dir);
        ret = system(shell_cmd);
        if(ret != 0){
            err_msg("mkdir failed\n");
            return -1;
        }
    }
    strftime(file_time, MAX_FILE_NAME_LEN, "%Y-%m-%d-%H-%M-%S", &disperse_time);
    memset(chn_param->file_name, 0, sizeof(chn_param->file_name));
#ifndef TS_TEST
    sprintf(chn_param->file_name, "%s/%s-%d.mkv",  file_pre_dir, file_time, chn_param->chn_num);
#else

    if(chn_param->chn_num == 0){
        sprintf(chn_param->file_name, "udp://192.168.1.221:22222",  file_pre_dir, file_time, chn_param->chn_num);
        //sprintf(chn_param->file_name, "%s/%s-%d.ts",  file_pre_dir, file_time, chn_param->chn_num);
        ret = avformat_alloc_output_context2(&chn_param->out_context, NULL, "mpegts", chn_param->file_name);
        if(ret < 0){
            err_msg("avformat_alloc_output_context2 failed\n");
            return -1;
        }
    }else{
        sprintf(chn_param->file_name, "%s/%s-%d.mkv",  file_pre_dir, file_time, chn_param->chn_num);
        ret = avformat_alloc_output_context2(&chn_param->out_context, NULL, NULL, chn_param->file_name);
        if(ret < 0){
            err_msg("avformat_alloc_output_context2 failed\n");
            return -1;
        }

    }
    //sprintf(chn_param->file_name, "%s/%s-%d.ts",  file_pre_dir, file_time, chn_param->chn_num);
#endif

#ifndef TS_TEST
    ret = avformat_alloc_output_context2(&chn_param->out_context, NULL, NULL, chn_param->file_name);
    if(ret < 0){
        err_msg("avformat_alloc_output_context2 failed\n");
        return -1;
    }
#endif

    ret =add_video_stream(chn_param);
    if(ret != 0){
        err_msg("add_video_stream failed\n");
        return -1;
    }

    if(chn_param->audio_enable == true){
        ret =add_audio_stream(chn_param);
        if(ret != 0){
            err_msg("add_video_stream failed\n");
            return -1;
        }
    }

    //av_dump_format(chn_param->out_context, 0, chn_param->file_name, 1);
    if (!(chn_param->out_context->oformat->flags & AVFMT_NOFILE))
    {
        //info_msg( "before avio_open\n");
        ret = avio_open(&chn_param->out_context->pb, chn_param->file_name, AVIO_FLAG_WRITE);
        if (ret < 0)
        {
            char buf[1024];
            av_strerror(ret, buf, sizeof(buf));
            printf("could not open '%s': %d, err = %s\n", chn_param->file_name, ret, buf);
            return 1;
        }
    }

    ret = avformat_write_header(chn_param->out_context, NULL);
    if (ret < 0) {
        err_msg( "Fail to write the header of output ");
        return 0;
    }
#if REGISTER_INDEX
    ret = create_file_index(chn_param->chn_num, chn_param->file_name, chn_param->audio_enable);
    if(ret != 0){
        err_msg("create_file_index failed\n");
    }
    chn_param->register_size_bound = REGISTER_FILE_SIZE_BOUND;

#endif

    chn_param->first_packet_pts = -1;

    return 0;
}

static int write_tailer_to_file(struct chn_param_s *chn_param)
{
    int ret;
    ret =av_write_trailer(chn_param->out_context);
    if(ret != 0){
        err_msg("av_write_trailer failed\n");
        return -1;
    }

    ret = avio_close(chn_param->out_context->pb);
    if(ret != 0){
        err_msg("avio_close failed\n");
    }

    avformat_free_context(chn_param->out_context);

#if REGISTER_INDEX
    ret = register_file_index_finished(chn_param->chn_num, chn_param->file_name);
    if(ret != 0){
        err_msg("register_file_index_finished failed\n");
        return -1;
    }
#endif
    return 0;
}

static inline int judge_write_tailer_clean_flag(struct chn_param_s *chn_param)
{
#if 1
    if(chn_param->out_context){
        write_tailer_to_file(chn_param);

        chn_param->first_packet_pts = -1;
        chn_param->out_context = NULL;
        chn_param->audio_stream = NULL;
        chn_param->video_stream = NULL;
        return 0;
    }
#endif
    return 1;
}

int switch_new_file(int chn, struct container_param_s *param)
{
    struct chn_param_s *chn_param = global_param->chn_params + chn;

    pthread_mutex_lock(&chn_param->mutex);

    judge_write_tailer_clean_flag(chn_param);

    //param_copy(chn_param, param);

    create_new_file(chn_param);

    pthread_mutex_unlock(&chn_param->mutex);
    return 0;
}

int stop_container(int chn)
{
    struct chn_param_s *chn_param = global_param->chn_params + chn;

    pthread_mutex_lock(&chn_param->mutex);

    judge_write_tailer_clean_flag(chn_param);

    pthread_mutex_unlock(&chn_param->mutex);
    return 0;
}

static int write_video_packet_to_file(struct chn_param_s *chn_param, Mal_StreamBlock *block, int64_t *pts)
{
    //printf("%s\n", __FUNCTION__);
    char *packet_buff = NULL;
    int packet_size = 0;
    AVPacket pkt;
    AVRational hi_pts_avrational;
    int ret;

    packet_buff = (char *)block->p_buffer;
    packet_size = block->i_buffer;

    hi_pts_avrational.den = 1000000;
    hi_pts_avrational.num = 1;

    av_init_packet(&pkt);

    pkt.pts = av_rescale_q(block->i_pts - chn_param->first_packet_pts, hi_pts_avrational, chn_param->video_stream->time_base);

    *pts = pkt.pts;


    pkt.dts = pkt.pts;
    pkt.data = (unsigned char *)packet_buff;
    pkt.size = packet_size;
    pkt.stream_index= chn_param->video_stream->index;
    if(block->i_flags == BLOCK_H264E_NALU_ISLICE ){
        pkt.flags = AV_PKT_FLAG_KEY;
    }

    if(pkt.pts >= chn_param->register_size_bound){
        register_file_size(chn_param->chn_num, chn_param->file_name);
        chn_param->register_size_bound += REGISTER_FILE_SIZE_BOUND;
    }

    ret = av_interleaved_write_frame(chn_param->out_context, &pkt);
    if(ret != 0){
        err_msg("av_interleaved_write_frame error video,chn_num = %d\n",chn_param->chn_num);
        return -1;
    }
    //info_msg("info pkt.pts = %l64d\n", pkt.pts);
    return 0;
}

static int write_audio_packet_to_file(struct chn_param_s *chn_param, Mal_StreamBlock *block)
{
    AVPacket pkt;
    AVRational hi_pts_avrational;
    int ret;

    hi_pts_avrational.den = 1000000;
    hi_pts_avrational.num = 1;

    av_init_packet(&pkt);

    if(chn_param->first_packet_pts == 0){
        chn_param->first_packet_pts = block->i_pts;
        pkt.pts = 0;
    }else{
        pkt.pts= av_rescale_q(block->i_pts - chn_param->first_packet_pts, hi_pts_avrational, chn_param->audio_stream->time_base);
    }

    pkt.dts= pkt.pts;
    if(pkt.size <= 4){
        return -1;
    }
    pkt.data = block->p_buffer + 4;
    pkt.size = block->i_buffer - 4;
    pkt.stream_index= chn_param->audio_stream->index;

    //ret = av_write_frame(mkv_info->out_context, &pkt);
    ret = av_interleaved_write_frame(chn_param->out_context, &pkt);
    if(ret != 0){
        printf("av_interleaved_write_frame error, ret = %d\n", ret);
        return -1;
    }

    return 0;
}

int container_start_new_file(char *file_name, int chn)
{
    int ret;
    struct chn_param_s *chn_param = global_param->chn_params + chn;
    //1.create new file or not
    pthread_mutex_lock(&chn_param->mutex);
    //info_msg("----------------------------------create_new_file----------------------\n");
    ret = create_new_file(chn_param);
    //info_msg("after create_new_file\n");
    if(ret != 0){
        err_msg("create_new_file failed \n");
        ret = -1;
        goto unlock_label;
    }

unlock_label:
    pthread_mutex_unlock(&chn_param->mutex);
    return 0;
}


int get_raw_stream_v(int chn, Mal_StreamBlock *block, __attribute__((unused)) void* opaque)
{
    struct chn_param_s *chn_param = global_param->chn_params + chn;

    pthread_mutex_lock(&chn_param->mutex);
    if(!chn_param->out_context || !chn_param->video_stream){
        pthread_mutex_unlock(&chn_param->mutex);
        return 0;
    }
    //3.write data to file
    if(chn_param->first_packet_pts == -1){
        chn_param->first_packet_pts = block->i_pts;
    }
    int64_t pts = 0;
    write_video_packet_to_file(chn_param, block, &pts);

    pthread_mutex_unlock(&chn_param->mutex);
#if 0
#if REGISTER_INDEX
    if(block->i_flags == BLOCK_H264E_NALU_ISLICE ){
        ret = register_file_piece(chn_param->chn_num, chn_param->file_name);
        if(ret != 0){
            err_msg("register_file_piece error\n");
        }
    }
#endif
#endif

    return 0;
}



int container_send_video( Mal_StreamBlock *block)
{
    get_raw_stream_v(block->chn_num, block, block->chn_num);
    return 0;
}

int container_send_audio( Mal_StreamBlock *block)
{
    get_raw_stream_a(block->chn_num, block, block->chn_num);
    return 0;
}

int get_raw_stream_a(__attribute__((unused)) int chn, Mal_StreamBlock *block, void* opaque)
{
    int video_chn = (int )opaque;

    struct chn_param_s *chn_param = global_param->chn_params + video_chn;

    pthread_mutex_lock(&chn_param->mutex);
    if(!chn_param->out_context || !chn_param->audio_stream){
        pthread_mutex_unlock(&chn_param->mutex);
        return 0;
    }
    write_audio_packet_to_file(chn_param, block);
    pthread_mutex_unlock(&chn_param->mutex);
    return 0;
}


int container_destory()
{
    int i;
    for(i = 0; i < 8; i++){
        pthread_mutex_destroy(&global_param->chn_params[i].mutex);
    }

    free(global_param->chn_params);
    free(global_param);
    return 0;
}

//---------------------------------------------audio resampler


#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>

static short _g711_decode(unsigned char alaw)
{
    alaw ^= 0xD5;
    int sign = alaw & 0x80;
    int exponent = (alaw & 0x70) >> 4;
    int data = alaw & 0x0f;
    data <<= 4;
    data += 8;
    if (exponent != 0)
        data += 0x100;
    if (exponent > 1)
        data <<= (exponent - 1);

    return (short)(sign == 0 ? data : -data);
}

static int g711_decode(char* pRawData, const unsigned char* pBuffer, int nBufferSize)
{
    short *out_data = (short*)pRawData;
    int i;
    for(i=0; i<nBufferSize; i++)
    {
        out_data[i] = _g711_decode(pBuffer[i]);
    }

    return nBufferSize*2;
}

int resample_main()
{
    int64_t src_ch_layout = AV_CH_LAYOUT_MONO, dst_ch_layout = AV_CH_LAYOUT_MONO;
    int src_rate = 8000, dst_rate = 11025;
    uint8_t **src_data = NULL, **dst_data = NULL;
    int src_nb_channels = 0, dst_nb_channels = 0;
    int src_linesize, dst_linesize;
    int src_nb_samples = 320, dst_nb_samples, max_dst_nb_samples;

    enum AVSampleFormat src_sample_fmt = AV_SAMPLE_FMT_S16, dst_sample_fmt = AV_SAMPLE_FMT_S16;

    char *dst_filename = "paomo_11025.pcm";
    FILE *dst_file;
    int dst_bufsize;
    struct SwrContext *swr_ctx;
    int ret;
    //char *src_filename = "paomo.pcm";
    char *src_filename = "paomo.alaw";
    FILE *src_file;


    dst_file = fopen(dst_filename, "wb");
    if (!dst_file) {
        fprintf(stderr, "Could not open destination file %s\n", dst_filename);
        exit(1);
    }

    src_file = fopen(src_filename, "rb");
    if (!src_file) {
        fprintf(stderr, "Could not open source file %s\n", src_filename);
        exit(1);
    }

    /* create resampler context */
    swr_ctx = swr_alloc();
    if (!swr_ctx) {
        fprintf(stderr, "Could not allocate resampler context\n");
        ret = AVERROR(ENOMEM);
        goto end;
    }

    /* set options */
    av_opt_set_int(swr_ctx, "in_channel_layout",    src_ch_layout, 0);
    av_opt_set_int(swr_ctx, "in_sample_rate",       src_rate, 0);
    av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", src_sample_fmt, 0);

    av_opt_set_int(swr_ctx, "out_channel_layout",    dst_ch_layout, 0);
    av_opt_set_int(swr_ctx, "out_sample_rate",       dst_rate, 0);
    av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", dst_sample_fmt, 0);

    /* initialize the resampling context */
    if ((ret = swr_init(swr_ctx)) < 0) {
        fprintf(stderr, "Failed to initialize the resampling context\n");
        goto end;
    }

    /* allocate source and destination samples buffers */
    src_nb_channels = av_get_channel_layout_nb_channels(src_ch_layout);
    ret = av_samples_alloc_array_and_samples(&src_data, &src_linesize, src_nb_channels,
                                             src_nb_samples, src_sample_fmt, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate source samples\n");
        goto end;
    }
    printf("src_linesize = %d\n", src_linesize);

    /* compute the number of converted samples: buffering is avoided
     * ensuring that the output buffer will contain at least all the
     * converted input samples */
    max_dst_nb_samples = dst_nb_samples =
        av_rescale_rnd(src_nb_samples, dst_rate, src_rate, AV_ROUND_UP);
    printf("max_dst_nb_samples  = %d\n", max_dst_nb_samples);

    /* buffer is going to be directly written to a rawaudio file, no alignment */
    dst_nb_channels = av_get_channel_layout_nb_channels(dst_ch_layout);
    ret = av_samples_alloc_array_and_samples(&dst_data, &dst_linesize, dst_nb_channels,
                                             dst_nb_samples, dst_sample_fmt, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate destination samples\n");
        goto end;
    }


    while(1){
        /* generate synthetic audio */
        char data_buf[10000] = {0};
        int data_size = src_nb_samples ;

        ret = fread(data_buf, 1, data_size, src_file);
        if(ret != data_size){
            printf("read file over\n");
            break;
        }

        g711_decode(src_data[0], data_buf, data_size);

        /* compute destination number of samples */
        dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx, src_rate) +
                                        src_nb_samples, dst_rate, src_rate, AV_ROUND_UP);
        printf("swr_get_delay = %d\n", swr_get_delay(swr_ctx, src_rate));

        if (dst_nb_samples > max_dst_nb_samples) {
            av_freep(&dst_data[0]);
            ret = av_samples_alloc(dst_data, &dst_linesize, dst_nb_channels,
                                   dst_nb_samples, dst_sample_fmt, 1);
            if (ret < 0)
                break;
            max_dst_nb_samples = dst_nb_samples;
        }

        /* convert to destination format */
        ret = swr_convert(swr_ctx, dst_data, dst_nb_samples, (const uint8_t **)src_data, src_nb_samples);
        if (ret < 0) {
            fprintf(stderr, "Error while converting\n");
            goto end;
        }
        dst_bufsize = av_samples_get_buffer_size(&dst_linesize, dst_nb_channels,
                                                 ret, dst_sample_fmt, 1);
        if (dst_bufsize < 0) {
            fprintf(stderr, "Could not get sample buffer size\n");
            goto end;
        }
        //printf("t:%f in:%d out:%d\n", t, src_nb_samples, ret);
        fwrite(dst_data[0], 1, dst_bufsize, dst_file);
    }


end:
    fclose(dst_file);

    if (src_data)
        av_freep(&src_data[0]);
    av_freep(&src_data);

    if (dst_data)
        av_freep(&dst_data[0]);
    av_freep(&dst_data);

    swr_free(&swr_ctx);
    return ret < 0;
}

