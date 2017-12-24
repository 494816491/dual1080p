#include <stdbool.h>
#include <libavformat/avformat.h>
#include <libavutil/avstring.h>
#include <libavutil/dict.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libavutil/avassert.h>
#include <pthread.h>
#include "debug.h"
#include "hi_common.h"
#include "iniparser.h"
#include "rtmp_h264.h"

static int audio_resample_init();
static int resample_main(char *g711a_data, int g711a_data_size, char **ret_buf, int *ret_buf_size);

//this must set before write header
static char ext_data[23] ={0x00,0x00,0x00,0x01,0x67,0x4D,0x00,0x2A,0x9D,0xA8,0x1E,0x00,0x89,0xF9,0x50,0x00,0x00,0x00,0x01,0x68,0xEE,0x3C,0x80};

#define RTMP_H264_CHN_NUM 1
#define RTMP_INI_PATH ""
#define RTMP_CLIENT_IP_KEY ""

struct rtmp_h264_st{
    char rtmp_ip[MAX_RTMP_URL_LEN];
    AVFormatContext *out_context;
    AVRational time_base;
    int video_frame_rate;
    int chn_num;
    AVStream *video_stream;

    SIZE_S video_size;
    int64_t last_pts;

    char sps_pps_buffer[1024];
    int used_buffer_size;

    //------------
    int audio_enable;
    int audio_sample_rate;
    int audio_bit_width;
    int audio_sound_mode;
    int audio_num_pre_frame;

    AVStream *audio_stream;
    //-----------------
    pthread_mutex_t mutex;

}rtmp_h264_info[RTMP_H264_CHN_NUM ];

int printf_codec_context(AVCodecContext *ctx)
{
    printf("av_class = %p\n", ctx->av_class);
    printf("codec = %p\n", ctx->codec);
    printf("codec_name = %s\n", ctx->codec_name);
    printf("codec_id = %d\n", ctx->codec_id);
    printf("av_class = %p\n", ctx->av_class);
    printf("av_class = %p\n", ctx->av_class);
    printf("av_class = %p\n", ctx->av_class);
    return 0;
}

//#if MAL_RTMP_SERVER
static int add_video_stream(struct rtmp_h264_st *rtmp_h264_data)
{
#if 1
    AVCodecContext *codec_context;

    rtmp_h264_data->video_stream = avformat_new_stream(rtmp_h264_data->out_context, NULL);
    if(rtmp_h264_data->video_stream == NULL){
        err_msg("avformat_new_stream error\n");
        return -1;
    }

    //Mandatory fields as specified in AVCodecContext documentation must be set even if this AVCodecContext is not actually used for encoding
    codec_context = rtmp_h264_data->video_stream->codec;
    //codec_context->codec_id = codec_id;
    codec_context->codec_id = AV_CODEC_ID_H264;
    codec_context->codec_type = AVMEDIA_TYPE_VIDEO;

    //must set width and height
    codec_context->width = rtmp_h264_data->video_size.u32Width;
    codec_context->height = rtmp_h264_data->video_size.u32Height;
    codec_context->coded_width = rtmp_h264_data->video_size.u32Width;
    codec_context->coded_height = rtmp_h264_data->video_size.u32Height;
    codec_context->pix_fmt = AV_PIX_FMT_YUV420P;
    codec_context->time_base.num = 1;
    codec_context->time_base.den = rtmp_h264_data->video_frame_rate;
    codec_context->gop_size = rtmp_h264_data->video_frame_rate;
#if 1
    codec_context->extradata = ext_data;
    codec_context->extradata_size = sizeof(ext_data);
#endif

    // some formats want stream headers to be separate
#if 1
    if(rtmp_h264_data->out_context->oformat->flags & AVFMT_GLOBALHEADER){
        codec_context->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }
#endif
#endif

    return 0;
end:
    exit(-1);
}

static int add_audio_stream(struct rtmp_h264_st *rtmp_h264_data)
{
    //int ret;
    AVStream *audio_stream;
    AVCodecContext *codec_context;

    rtmp_h264_data->audio_stream = avformat_new_stream(rtmp_h264_data->out_context, NULL);
    if(rtmp_h264_data->audio_stream == NULL){
        err_msg( "avformat_new_stream error,audio_stream = %p\n", rtmp_h264_data->audio_stream);
        return -1;
    }

    audio_stream = rtmp_h264_data->audio_stream;

    //printf("before set codec par\n");
    codec_context = audio_stream->codec;
    //AV_CODEC_ID_PCM_ALAW

    //codec_context->bits_per_coded_sample = AV_SAMPLE_FMT_S16;

    //codec_context->codec_id = AV_CODEC_ID_PCM_ALAW;
    codec_context->codec_id = AV_CODEC_ID_PCM_S16LE;
    codec_context->codec_type = AVMEDIA_TYPE_AUDIO;

    codec_context->sample_rate = rtmp_h264_data->audio_sample_rate;
    codec_context->channels = 1;
    //codec_context->sample_fmt = AV_SAMPLE_FMT_S16;
    //codec_context->bits_per_coded_sample = 8;
    codec_context->bits_per_coded_sample = 16;
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
    if(rtmp_h264_data->out_context->oformat->flags & AVFMT_GLOBALHEADER){
        codec_context->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }
    //printf("after set codec par\n");

    return 0;
}

int set_rtmp_chn_param(int chn, struct rtmp_chn_param_st *param)
{
    if(chn >= RTMP_H264_CHN_NUM){
        err_msg("chn can not larger than RTMP_H264_CHN_NUM");
        exit(-1);
    }

    struct rtmp_h264_st *rtmp_h264_data = &rtmp_h264_info[chn];

    pthread_mutex_lock(&rtmp_h264_data->mutex);

    sprintf(rtmp_h264_data->rtmp_ip, param->ip_addr);

    rtmp_h264_data->video_frame_rate = param->video_frame_rate;
    rtmp_h264_data->chn_num = chn;
    rtmp_h264_data->last_pts = 0;

    rtmp_h264_data->video_size.u32Width = param->video_w;
    rtmp_h264_data->video_size.u32Height = param->video_h;

    rtmp_h264_data->time_base.num = 1;
    rtmp_h264_data->time_base.den = rtmp_h264_data->video_frame_rate;

    rtmp_h264_data->audio_bit_width = param->audio_bit_width;
    rtmp_h264_data->audio_num_pre_frame = param->audio_num_pre_frame;
    rtmp_h264_data->audio_sample_rate = param->audio_sample_rate;
    rtmp_h264_data->audio_sound_mode = param->audio_sound_mode;
    rtmp_h264_data->audio_enable = param->audio_enable;

    pthread_mutex_unlock(&rtmp_h264_data->mutex);
    return 0;
}

int open_rtmp_stream(int chn)
{
    int ret;
    struct rtmp_h264_st *rtmp_h264_data = &rtmp_h264_info[chn];
    pthread_mutex_lock(&rtmp_h264_data->mutex);


    ret = avformat_alloc_output_context2(&rtmp_h264_data->out_context, NULL, "flv", rtmp_h264_data->rtmp_ip);
    if(ret < 0){
        err_msg("avformat_alloc_output_context2 failed,%s\n", av_err2str(ret));
        goto end_label;
    }

    add_video_stream(rtmp_h264_data);

    if(rtmp_h264_data->audio_enable){
        add_audio_stream(rtmp_h264_data);
    }

    if (!(rtmp_h264_data->out_context->oformat->flags & AVFMT_NOFILE))
    {
        //info_msg( "before avio_open\n");
        AVDictionary *avdic=NULL;
        char option_key[]="rtmp_buffer";
        char option_value[]="50";
        av_dict_set(&avdic,option_key,option_value,0);

        ret = avio_open2(&rtmp_h264_data->out_context->pb, rtmp_h264_data->rtmp_ip, AVIO_FLAG_WRITE, NULL, &avdic);
        if (ret < 0)
        {
            char buf[1024];
            av_strerror(ret, buf, sizeof(buf));
            printf("could not open '%s': %d, err = %s\n", rtmp_h264_data->rtmp_ip, ret, buf);
            goto end_label;
        }
    }
    ret = avformat_write_header(rtmp_h264_data->out_context, NULL);
    if (ret < 0) {
        err_msg( "Fail to write the header of output ");
        goto end_label;
    }
end_label:
    pthread_mutex_unlock(&rtmp_h264_data->mutex);
    return 0;
}


int rtmp_h264_server_init()
{
    info_msg("rtmp_h264_server_init");
    int i = 0;

    ffmpeg_init();
    for(i = 0; i < RTMP_H264_CHN_NUM; i++){
        struct rtmp_h264_st *rtmp_h264_data = &rtmp_h264_info[i];
        pthread_mutex_init(&rtmp_h264_data->mutex, NULL);
    }
    audio_resample_init();
    return 0;
}

int send_rtmp_video_stream(Mal_StreamBlock *block)
{
    struct rtmp_h264_st *rtmp_h264_data = &rtmp_h264_info[block->chn_num];

    //info_msg("before send_rtmp_video_stream");
    //judge is context is ok
    pthread_mutex_lock(&rtmp_h264_data->mutex);
    if(!rtmp_h264_data->out_context || !rtmp_h264_data->video_stream){
        pthread_mutex_unlock(&rtmp_h264_data->mutex);
        return 0;
    }
    pthread_mutex_unlock(&rtmp_h264_data->mutex);
    //info_msg("after send_rtmp_video_stream");

    char *packet_buff = NULL;
    int packet_size = 0;
    AVPacket pkt;
    AVRational hi_pts_avrational;
    int ret;

    packet_buff = (char *)block->p_buffer;
    packet_size = block->i_buffer;

    //hi_pts_avrational.den = 1000000;
    hi_pts_avrational.den = 1000;
    hi_pts_avrational.num = 1;

#if 1
    int should_release_i_frame  = 0;
    if(*(packet_buff + 4) == 0x67){
        //判断是否为sps和pps将其放到缓存中，之后进行组合
        memset(rtmp_h264_data->sps_pps_buffer, 0, sizeof(rtmp_h264_data->sps_pps_buffer));
        memcpy(rtmp_h264_data->sps_pps_buffer, packet_buff, packet_size);
        rtmp_h264_data->used_buffer_size  = packet_size;
        return 0;
    }else if(*(packet_buff + 4) == 0x68){
        memcpy(rtmp_h264_data->sps_pps_buffer + rtmp_h264_data->used_buffer_size, packet_buff, packet_size);
        rtmp_h264_data->used_buffer_size += packet_size;
        return 0;
    }else if(*(packet_buff + 4) == 0x06){
        return 0;
    }else if(*(packet_buff + 4) == 0x65){
        char *tmp = packet_buff;
        int tmp_size = packet_size;
        packet_size += rtmp_h264_data->used_buffer_size;			//媒体数
        packet_buff = malloc(packet_size);			//媒体数据缓冲区（原始码流数据
        memcpy(packet_buff, rtmp_h264_data->sps_pps_buffer, rtmp_h264_data->used_buffer_size);
        memcpy(packet_buff + rtmp_h264_data->used_buffer_size, tmp, tmp_size);

        should_release_i_frame  = 1;
    }
#endif

    av_init_packet(&pkt);
    pkt.data = (unsigned char *)packet_buff;
    pkt.size = packet_size;
    pkt.stream_index= rtmp_h264_data->video_stream->index;

    if(block->i_flags == BLOCK_H264E_NALU_ISLICE ){
        pkt.flags = AV_PKT_FLAG_KEY;
    }
    //info_msg("pkt.data = %p, size = %d, stream_index = %d\n", pkt.data, pkt.size, pkt.stream_index);
    //info_msg("rtmp_h264_data->out_context = %p", rtmp_h264_data->out_context);

    pkt.pts = block->i_pts / 1000;
    pkt.dts = pkt.pts;

    pthread_mutex_lock(&rtmp_h264_data->mutex);
        ret = av_interleaved_write_frame(rtmp_h264_data->out_context, &pkt);

        if(ret != 0){
            err_msg("av_interleaved_write_frame error video,chn_num = %d\n",rtmp_h264_data->chn_num);

            char buf[1024];
            av_strerror(ret, buf, sizeof(buf));
            printf("could not open : %d, err = %s\n",  ret, buf);
            pthread_mutex_unlock(&rtmp_h264_data->mutex);
            return -1;
        }
    pthread_mutex_unlock(&rtmp_h264_data->mutex);

    if(should_release_i_frame){
        free(packet_buff);
    }

    //info_msg("after send_rtmp_video_stream");

    return 0;
}

int send_rtmp_audio_stream( Mal_StreamBlock *block)
{
    //info_msg("get_raw_stream_a video_chn = %d\n", block->chn_num);

    //struct chn_param_s *chn_param = global_param->chn_params + chn;
    struct rtmp_h264_st *rtmp_h264_data = &rtmp_h264_info[block->chn_num];

    //judge is context is ok
    pthread_mutex_lock(&rtmp_h264_data->mutex);
    if(!rtmp_h264_data->out_context || !rtmp_h264_data->audio_stream){
        pthread_mutex_unlock(&rtmp_h264_data->mutex);
        return 0;
    }
    pthread_mutex_unlock(&rtmp_h264_data->mutex);

    AVPacket pkt;
    AVRational hi_pts_avrational;
    int ret;

    //hi_pts_avrational.den = 1000000;
    hi_pts_avrational.den = 1000;
    hi_pts_avrational.num = 1;

    av_init_packet(&pkt);
#if 0
    pkt.data = block->p_buffer;
    pkt.size = block->i_buffer;
#else
    resample_main(block->p_buffer, block->i_buffer, &pkt.data, &pkt.size);
    if(pkt.size == 0){
        info_msg("resample_main no buffer size");
        return 0;
    }
#endif
    pkt.stream_index= rtmp_h264_data->audio_stream->index;

    pkt.pts = block->i_pts / 1000;

    pkt.dts = pkt.pts;
    //info_msg("pkt.data = %p, size = %d, stream_index = %d\n", pkt.data, pkt.size, pkt.stream_index);


    pthread_mutex_lock(&rtmp_h264_data->mutex);
    //info_msg("av_interleaved_write_frame");
    ret = av_interleaved_write_frame(rtmp_h264_data->out_context, &pkt);
    if(ret != 0){
        pthread_mutex_unlock(&rtmp_h264_data->mutex);
        err_msg("av_interleaved_write_frame error audio,chn_num = %d\n",rtmp_h264_data->chn_num);

        char buf[1024];
        av_strerror(ret, buf, sizeof(buf));
        printf("could not open : %d, err = %s\n",  ret, buf);
        return -1;
    }
    pthread_mutex_unlock(&rtmp_h264_data->mutex);

    return 0;
}
//#endif


int close_rtmp_stream(int chn)
{
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
struct resample_info_st{
    int max_dst_nb_samples;

    uint8_t **src_data;
    uint8_t **dst_data;
    int src_linesize;
    int dst_linesize;
    int src_nb_channels;
    int dst_nb_channels;
    enum AVSampleFormat src_sample_fmt;
    enum AVSampleFormat dst_sample_fmt;

    //init
    struct SwrContext *swr_ctx;
    //----------config
    int src_rate;
    int dst_rate;
    int64_t src_ch_layout;
    int64_t dst_ch_layout;

    int src_nb_samples;
    int dst_nb_samples;
    int dst_bufsize;

}resample_info;

static int audio_resample_init()
{
    struct resample_info_st *info = &resample_info;
    int ret;

    info->src_ch_layout = AV_CH_LAYOUT_MONO;
    info->dst_ch_layout = AV_CH_LAYOUT_MONO;
    info->src_rate = 8000;
    info->dst_rate = 11025;

    info->src_sample_fmt = AV_SAMPLE_FMT_S16;
    info->dst_sample_fmt = AV_SAMPLE_FMT_S16;
    info->src_nb_samples = 320;
#if 0
    char *dst_filename = "paomo_11025.pcm";
    FILE *dst_file;

    dst_file = fopen(dst_filename, "wb");
    if (!dst_file) {
        fprintf(stderr, "Could not open destination file %s\n", dst_filename);
        exit(1);
    }
#endif
    /* create resampler context */
    info->swr_ctx = swr_alloc();
    if (!info->swr_ctx) {
        err_msg("Could not allocate resampler context");
        ret = AVERROR(ENOMEM);
        return -1;
    }

    /* set options */
    av_opt_set_int(info->swr_ctx, "in_channel_layout",    info->src_ch_layout, 0);
    av_opt_set_int(info->swr_ctx, "in_sample_rate",       info->src_rate, 0);
    av_opt_set_sample_fmt(info->swr_ctx, "in_sample_fmt", info->src_sample_fmt, 0);

    av_opt_set_int(info->swr_ctx, "out_channel_layout",    info->dst_ch_layout, 0);
    av_opt_set_int(info->swr_ctx, "out_sample_rate",       info->dst_rate, 0);
    av_opt_set_sample_fmt(info->swr_ctx, "out_sample_fmt", info->dst_sample_fmt, 0);

    /* initialize the resampling context */
    if ((ret = swr_init(info->swr_ctx)) < 0) {
        fprintf(stderr, "Failed to initialize the resampling context\n");
    }

    /* allocate source and destination samples buffers */
    info->src_nb_channels = av_get_channel_layout_nb_channels(info->src_ch_layout);
    ret = av_samples_alloc_array_and_samples(&info->src_data, &info->src_linesize, info->src_nb_channels,
                                             info->src_nb_samples, info->src_sample_fmt, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate source samples\n");
        return -1;
    }
    printf("src_linesize = %d\n", info->src_linesize);

    /* compute the number of converted samples: buffering is avoided
     * ensuring that the output buffer will contain at least all the
     * converted input samples */
    info->max_dst_nb_samples = info->dst_nb_samples =
        av_rescale_rnd(info->src_nb_samples, info->dst_rate, info->src_rate, AV_ROUND_UP);
    printf("max_dst_nb_samples  = %d\n", info->max_dst_nb_samples);

    /* buffer is going to be directly written to a rawaudio file, no alignment */
    info->dst_nb_channels = av_get_channel_layout_nb_channels(info->dst_ch_layout);
    ret = av_samples_alloc_array_and_samples(&info->dst_data, &info->dst_linesize, info->dst_nb_channels,
                                             info->dst_nb_samples, info->dst_sample_fmt, 0);
    if (ret < 0) {
        err_msg("Could not allocate destination samples\n");
    }
    return -1;
}

static int resample_main(char *g711a_data, int g711a_data_size, char **ret_buf, int *ret_buf_size)
{
    int ret;
    struct resample_info_st *info = &resample_info;
    /* generate synthetic audio */
    if(g711a_data_size != info->src_nb_samples){
        info_msg("g711a_data_size != info->src_nb_samples , g711a_data_size = %d", g711a_data_size);
        exit(-1);
    }


    g711_decode(info->src_data[0], g711a_data, g711a_data_size);

    /* compute destination number of samples */
    info->dst_nb_samples = av_rescale_rnd(swr_get_delay(info->swr_ctx, info->src_rate) +
                                          info->src_nb_samples, info->dst_rate, info->src_rate, AV_ROUND_UP);
    //printf("swr_get_delay = %d\n", swr_get_delay(info->swr_ctx, info->src_rate));

    if (info->dst_nb_samples > info->max_dst_nb_samples) {
        av_freep(&info->dst_data[0]);
        ret = av_samples_alloc(info->dst_data, &info->dst_linesize, info->dst_nb_channels,
                               info->dst_nb_samples, info->dst_sample_fmt, 1);
        if (ret < 0){
            err_msg("av_samples_alloc failed");
            return -1;
        }
        info->max_dst_nb_samples = info->dst_nb_samples;
    }

    /* convert to destination format */
    ret = swr_convert(info->swr_ctx, info->dst_data, info->dst_nb_samples, (const uint8_t **)info->src_data, info->src_nb_samples);
    if (ret < 0) {
        fprintf(stderr, "Error while converting\n");
        return -1;
    }
    info->dst_bufsize = av_samples_get_buffer_size(&info->dst_linesize, info->dst_nb_channels,
                                             ret, info->dst_sample_fmt, 1);
    if (info->dst_bufsize < 0) {
       fprintf(stderr, "Could not get sample buffer size\n");
       return -1;
    }
    //等待一次8000/25 = 320, 11025/25 = 441,写入一次，时间戳按照上一次的才算
    if(info->dst_bufsize > 0){
        *ret_buf = info->dst_data[0];
        *ret_buf_size = info->dst_bufsize;
    }else{
        *ret_buf_size = NULL;
        *ret_buf_size = 0;
    }
#if 0
    //printf("t:%f in:%d out:%d\n", t, src_nb_samples, ret);
    fwrite(dst_data[0], 1, dst_bufsize, dst_file);
#endif

    return ret;
}

static int audio_resample_destory()
{
    struct resample_info_st *info = &resample_info;

    if (info->src_data)
        av_freep(&info->src_data[0]);
    av_freep(&info->src_data);

    if (info->dst_data)
        av_freep(&info->dst_data[0]);
    av_freep(&info->dst_data);

    swr_free(&info->swr_ctx);

#if 0
    fclose(info->dst_file);
#endif
    return 0;
}
