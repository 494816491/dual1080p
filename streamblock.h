#ifndef STREAM_BLOCK_H
#define STREAM_BLOCK_H

#include <stdint.h> 

#if 0
#define BLOCK_FLAG_TYPE_I               0x0002      ///< I帧
#define BLOCK_FLAG_TYPE_P               0x0004      ///< P帧
#define BLOCK_FLAG_TYPE_B               0x0008      ///< B帧
#define BLOCK_FLAG_TYPE_PB              0x0010      ///< P/B帧
#define BLOCK_FLAG_HEADER               0x0020      ///< 含有头部信息
#else

//改变了H264E_NALU_TYPE_E的名字，在mal外面应该使用这种类型
enum BLOCK_FLAG_TYPE{
	BLOCK_H264E_NALU_PSLICE = 1,
	BLOCK_H264E_NALU_ISLICE = 5,
	BLOCK_H264E_NALU_SEI = 6,
	BLOCK_H264E_NALU_SPS = 7,
	BLOCK_H264E_NALU_PPS = 8,
	BLOCK_H264E_NALU_BUTT,
};
#endif


#define MakeFourCC( a, b, c, d ) \
    ( ((uint32_t)a) | ( ((uint32_t)b) << 8 ) \
    | ( ((uint32_t)c) << 16 ) | ( ((uint32_t)d) << 24 ) )

#define AV_FOURCC_NULL  MakeFourCC('N', 'U', 'L', 'L')
#define AV_FOURCC_AAC   MakeFourCC('m', 'p', '4', 'a')
#define AV_FOURCC_JPEG  MakeFourCC('J', 'P', 'E', 'G')
#define AV_FOURCC_H264  MakeFourCC('a', 'v', 'c', '1')
#define AV_FOURCC_G711A   MakeFourCC('7', '1', '1', 'a')

#if 1
typedef struct _Mal_StreamBlock
{
    uint32_t        chn_num;
    uint8_t*        p_buffer;       ///< 负载数据起始位置
    uint32_t        i_buffer;       ///< 负载数据长度

    uint32_t        i_flags;        ///< 数据包类型，是I帧，P帧
    uint32_t        i_nb_samples;   ///< 音频样本数量
    uint32_t        i_track;        ///< 数据轨道
    uint32_t        i_codec;        ///< 编码类型FOURCC('a','b','c','d')

    int64_t         i_pts;          ///< 数据包时戳
    int64_t         i_dts;          ///< 解码时戳
    int64_t		    i_length;       ///< 持续时长
}Mal_StreamBlock;
#else
typedef struct _Mal_StreamBlock
{
	int type; //0:video, 1:audio
	union {
	   VENC_STREAM_S *video_stream;
	   AUDIO_STREAM_S *audio_stream;	
	};
}Mal_StreamBlock;

#endif




#endif //
