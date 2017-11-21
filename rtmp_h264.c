#include <stdbool.h>
#include <libavformat/avformat.h>
#include <libavutil/avstring.h>
#include <libavutil/dict.h>
#include <libavutil/opt.h>
#include <libavutil/time.h>
#include <libavutil/avassert.h>
#include "debug.h"
#include "hi_common.h"
#include "iniparser.h"
#include "rtmp_h264.h"
#include <hi_comm_venc.h>


//#define RTMP_H264_SERVER_IP "rtmp://192.168.1.201:2000%d"
//char RTMP_H264_SERVER_IP[100] = "rtmp://192.168.22.3/chn%d";
//char RTMP_H264_SERVER_IP[100] = "rtmp://192.168.22.2/chn%d";
//char RTMP_H264_SERVER_IP[100] = "rtmp://192.168.22.2/publishlive/livestream";
//char RTMP_H264_SERVER_IP[100] = "rtmp://192.168.22.2:6666/live/chn%d";
char RTMP_H264_SERVER_IP[100] = "rtmp://192.168.22.2/live/chn%d";

//this must set before write header
static char ext_data[23] ={0x00,0x00,0x00,0x01,0x67,0x4D,0x00,0x2A,0x9D,0xA8,0x1E,0x00,0x89,0xF9,0x50,0x00,0x00,0x00,0x01,0x68,0xEE,0x3C,0x80};

#define RTMP_H264_CHN_NUM 1
#define RTMP_INI_PATH ""
#define RTMP_CLIENT_IP_KEY ""


static int get_rtmp_chn_ip()
{
    int i;
    dictionary *dic = iniparser_load(RTMP_INI_PATH);
    if(dic == NULL){
        err_msg("iniparser_load failed\n");
        return -1;
    }
    char *ip = iniparser_getstring(dic, "General:"RTMP_CLIENT_IP_KEY, NULL);
    if(ip == NULL){
        err_msg("iniparser_getstring failed\n");
        return -1;
    }

    sprintf(RTMP_H264_SERVER_IP, "rtmp://%s:2000%s", ip, "%d");
    info_msg("RTMP_H264_SERVER_IP = %s\n", RTMP_H264_SERVER_IP);


    iniparser_freedict(dic);
    return 0;
}

struct rtmp_h264_st{
    char rtmp_ip[100];
    AVFormatContext *out_context;
    AVRational time_base;
    int video_frame_rate;
    int chn_num;
    AVStream *video_stream;

    SIZE_S video_size;
    int64_t last_pts;

    char sps_pps_buffer[1024];
    int used_buffer_size;

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

    codec_context->codec_type = AVMEDIA_TYPE_VIDEO;
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

int rtmp_h264_server_start()
{
    int i, ret;
    int video_frame_rate = 25;
    //get_rtmp_chn_ip();
	av_log_set_level(56);

    av_register_all();
    avformat_network_init();

    for(i = 0; i < RTMP_H264_CHN_NUM; i++){
        struct rtmp_h264_st *rtmp_h264_data = &rtmp_h264_info[i];
        sprintf(rtmp_h264_data->rtmp_ip, RTMP_H264_SERVER_IP,  i);

        rtmp_h264_data->video_frame_rate = video_frame_rate;
        rtmp_h264_data->chn_num = i;
        rtmp_h264_data->last_pts = 0;

        rtmp_h264_data->video_size.u32Width = 1920;
        rtmp_h264_data->video_size.u32Height = 1080;

        rtmp_h264_data->time_base.num = 1;
        rtmp_h264_data->time_base.den = rtmp_h264_data->video_frame_rate;

        info_msg("rtmp_h264_server_start  rtmp_ip = %s\n", rtmp_h264_data->rtmp_ip);

        ret = avformat_alloc_output_context2(&rtmp_h264_data->out_context, NULL, "flv", rtmp_h264_data->rtmp_ip);
        //ret = avformat_alloc_output_context2(&rtmp_h264_data->out_context, NULL, "mpegts", rtmp_h264_data->rtmp_ip);
        if(ret < 0){
            err_msg("avformat_alloc_output_context2 failed,%s\n", av_err2str(ret));
            return -1;
        }

        add_video_stream(rtmp_h264_data);

        if (!(rtmp_h264_data->out_context->oformat->flags & AVFMT_NOFILE))
        {
            //info_msg( "before avio_open\n");
#if 0
			info_msg("after avio_open2\n");

            ret = avio_open(&rtmp_h264_data->out_context->pb, rtmp_h264_data->rtmp_ip, AVIO_FLAG_WRITE);
            if (ret < 0)
            {
                char buf[1024];
                av_strerror(ret, buf, sizeof(buf));
                printf("could not open '%s': %d, err = %s\n", rtmp_h264_data->rtmp_ip, ret, buf);
                return 1;
            }
#else
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
                return 1;
            }

#endif
        }

    ret = avformat_write_header(rtmp_h264_data->out_context, NULL);
    if (ret < 0) {
        err_msg( "Fail to write the header of output ");
        return 0;
    }

    }

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
		send_rtmp_video_stream(&block);

		free(block.p_buffer);
	}
}

int send_rtmp_video_stream(Mal_StreamBlock *block)
{
    //printf("%s\n", __FUNCTION__);

    struct rtmp_h264_st *rtmp_h264_data = &rtmp_h264_info[block->chn_num];

    char *packet_buff = NULL;
    int packet_size = 0;
    AVPacket pkt;
    AVRational hi_pts_avrational;
    int ret;

    packet_buff = (char *)block->p_buffer;
    packet_size = block->i_buffer;
	//info_msg("packet_size = %d", packet_size);

    hi_pts_avrational.den = 1000000;
    hi_pts_avrational.num = 1;

    av_init_packet(&pkt);
    //printf("chn num = %d, pts = %lld\n", block->chn_num, pkt.pts);

    int should_release_i_frame  = 0;

#if 1
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

    pkt.data = (unsigned char *)packet_buff;
    pkt.size = packet_size;
    pkt.stream_index= rtmp_h264_data->video_stream->index;
    if(block->i_flags == BLOCK_H264E_NALU_ISLICE ){
        pkt.flags = AV_PKT_FLAG_KEY;
    }
	//info_msg("pkt.data = %p, size = %d, stream_index = %d\n", pkt.data, pkt.size, pkt.stream_index);

	//info_msg("rtmp_h264_data->out_context = %p", rtmp_h264_data->out_context);
#if 0
	rtmp_h264_data->video_stream->codec->extradata = ext_data;
	rtmp_h264_data->video_stream->codec->extradata_size = sizeof(ext_data);
#endif
	pkt.pts = block->i_pts / 1000;
#if 0
	if(rtmp_h264_data->last_pts < pkt.pts){
		rtmp_h264_data->last_pts = pkt.pts;
	}else if(rtmp_h264_data->last_pts == pkt.pts){
		rtmp_h264_data->last_pts += 33;
		pkt.pts =  rtmp_h264_data->last_pts;
	}else{
	}
#endif

    pkt.dts = pkt.pts;


    ret = av_interleaved_write_frame(rtmp_h264_data->out_context, &pkt);
    if(ret != 0){
        err_msg("av_interleaved_write_frame error video,chn_num = %d\n",rtmp_h264_data->chn_num);

		char buf[1024];
		av_strerror(ret, buf, sizeof(buf));
		printf("could not open : %d, err = %s\n",  ret, buf);
        return -1;
    }
	
    if(should_release_i_frame){
        free(packet_buff);
    }

    return 0;
}
//#endif

