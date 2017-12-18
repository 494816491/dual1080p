#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <stdint.h>
#include <mpi_region.h>
#include <pthread.h>
#include <time.h>
#include "osd.h"
#include "debug.h"
#include "text2bitmap.h"
#include "common.h"
//#include "hi_mem.h"

#ifndef ROUND_UP
#define ROUND_UP(size, align) (((size) + ((align) - 1)) & ~((align) - 1))
#endif

#ifndef ROUND_DOWN
#define ROUND_DOWN(size, align) ((size) & ~((align) - 1))
#endif

const wchar_t *digits = L"0123456789";
#define  TOTAL_DIGIT_NUM	 (10)
#define  DIGITS_BITMAP_SIZE  (2*11*48*48)
#define  STRING_SIZE    	 (32)
#define  STRING_BITMAP_SIZE  (2*32*48*48)


struct time_display
{
	char time_string[32];
	int digits_width;
	int digits_height;
	uint8_t digits_bitmap[DIGITS_BITMAP_SIZE];
	int bitmap_width;
	int bitmap_height;
	uint8_t time_bitmap[STRING_BITMAP_SIZE];
};

static struct time_display time_displays;

int gray2argb1555(const uint8_t* src, int src_len, uint8_t* dst, int dst_len)
{
	int i;
	uint16_t *rgb1555;

	if(!src || !dst)
		return -1;

	if(src_len*2 > dst_len) {
		printf("dst len is small than 2*src");
		return -1;
	}

	rgb1555 = (uint16_t*)dst;	
	for(i=0 ;i<src_len; i++){
		rgb1555[i] = src[i] == 0 ? 0 : 
		(0x8000 | ((0x1f&src[i]) << 10) | 
		((0x1f&src[i])<<5) | (0x1f&src[i]));
	}
	return 0;	
}

static int digits_convert(struct time_display* display, int font_size)
{
	uint16_t line_width, line_height, line_size;
	uint8_t *line_addr;
	int offset_x=0, max_width =0, i, error;
	wchar_t *p;
	bitmap_info_t bitmap_info;

	if(!display || font_size >48) {
		printf("invaild param or NULL ptr\n");
		return -1;
	}
	line_width = font_size*TOTAL_DIGIT_NUM;
	line_height = font_size;
	line_size = line_width * line_height;
	line_addr = (uint8_t*)malloc(line_size);
	if(line_addr == NULL) {
		printf("malloc failed\n");
		return -1;
	}
	memset(line_addr, 0, line_size);
	for (i = 0, p = digits; i < TOTAL_DIGIT_NUM; i++, p++) {
		if (text2bitmap_convert_character(*p, line_addr, line_height,
				line_width, offset_x, &bitmap_info) < 0) {
			printf("text2bitmap_convert_character failed\n");
			return -1;
		}
//		offset_x += digit_width;
//		printf("bitmap_info.width = %d\n", bitmap_info.width);
		offset_x += bitmap_info.width;
		if (bitmap_info.width > max_width)
			max_width = bitmap_info.width;
	}

	display->digits_width = line_width;
	display->digits_height = line_height;
	error = gray2argb1555(line_addr, line_size, display->digits_bitmap, DIGITS_BITMAP_SIZE);
	free(line_addr);
	return error;

}

static int freeType_osd_init(void)
{
	if(text2bitmap_lib_init(NULL) !=0) 
        return -1;

	font_attribute_t font;
    memset(&font, 0, sizeof(font));
    memset(&time_displays, 0, sizeof(time_displays));
    mal_strcpy_s(font.type, sizeof(font.type), OSD_FONTS_FILE_PATH);
	font.size = 30;
	font.outline_width = 0;
	font.hori_bold = 0;
	font.italic = 0;
	font.disable_anti_alias = 0;

	if(text2bitmap_set_font_attribute(&font)!=0) {
        text2bitmap_lib_deinit();
		return -1;
	}

	if(digits_convert(&time_displays, font.size) !=0) {
		text2bitmap_lib_deinit();
		return -1;
	}

    return 0;  
}


static int freeType_exit(void)
{
	return text2bitmap_lib_deinit();
}


int osd_open()
{
    int i, error;
	RGN_HANDLE handle;
	VI_CHN viChn;
    RGN_ATTR_S stOverlayExAttr;
    MPP_CHN_S connect_chn;
    RGN_CHN_ATTR_S stOverlayExChnAttr;

	if(freeType_osd_init() !=0)
		return -1;
    for(i=0; i<MAX_OSD_SUPPORT_AREARS; i++) {
			handle = i;	
            info_msg("handle = %d", handle);
            viChn = i;
#if 1
			stOverlayExAttr.enType = OVERLAYEX_RGN;
			stOverlayExAttr.unAttr.stOverlayEx.enPixelFmt = PIXEL_FORMAT_RGB_1555;
            //stOverlayExAttr.unAttr.stOverlayEx.stSize.u32Height = 32;
            stOverlayExAttr.unAttr.stOverlayEx.stSize.u32Height = 32;
            stOverlayExAttr.unAttr.stOverlayEx.stSize.u32Width= 500;
            stOverlayExAttr.unAttr.stOverlayEx.u32BgColor =  0x00000000;
#else
            stOverlayExAttr.enType = OVERLAY_RGN ;
            stOverlayExAttr.unAttr.stOverlay.enPixelFmt = PIXEL_FORMAT_RGB_1555;
            //stOverlayExAttr.unAttr.stOverlayEx.stSize.u32Height = 32;
            stOverlayExAttr.unAttr.stOverlay.stSize.u32Height = 400;
            stOverlayExAttr.unAttr.stOverlay.stSize.u32Width= 700;
            stOverlayExAttr.unAttr.stOverlay.u32BgColor =  0x00000000;
#endif

			if((error = HI_MPI_RGN_Create(handle, &stOverlayExAttr))!= HI_SUCCESS) {
                err_msg("HI_MPI_RGN_Create failed handle = %d 0x%x\n", handle, error);
				return -1;
			}
            info_msg("after HI_MPI_RGN_Create");

#if 1
            stOverlayExChnAttr.bShow = HI_TRUE;
            stOverlayExChnAttr.enType = OVERLAYEX_RGN;
            stOverlayExChnAttr.unChnAttr.stOverlayExChn.u32Layer = 1;
            stOverlayExChnAttr.unChnAttr.stOverlayExChn.u32FgAlpha = 255;
            stOverlayExChnAttr.unChnAttr.stOverlayExChn.u32BgAlpha = 255;
            stOverlayExChnAttr.unChnAttr.stOverlayExChn.stPoint.s32X = 1300;
            stOverlayExChnAttr.unChnAttr.stOverlayExChn.stPoint.s32Y = 900;
#else
            stOverlayExChnAttr.bShow = HI_TRUE;
            stOverlayExChnAttr.enType = OVERLAY_RGN;
            stOverlayExChnAttr.unChnAttr.stOverlayChn.stPoint.s32X = 800;
            stOverlayExChnAttr.unChnAttr.stOverlayChn.stPoint.s32Y = 800;
            stOverlayExChnAttr.unChnAttr.stOverlayChn.u32FgAlpha = 255;
            stOverlayExChnAttr.unChnAttr.stOverlayChn.u32BgAlpha = 255;
            stOverlayExChnAttr.unChnAttr.stOverlayChn.u32Layer = 1;
#endif
            connect_chn.enModId = HI_ID_VPSS;
            connect_chn.s32DevId = 1;
            connect_chn.s32ChnId = 3;

            error = HI_MPI_RGN_AttachToChn(handle, &connect_chn, &stOverlayExChnAttr);
			if(HI_SUCCESS != error){
                err_msg("HI_MPI_RGN_AttachToChn failed with  %#x! handle = %d\n", error, handle);
				return -1;
			}
            info_msg("HI_MPI_RGN_AttachToChn");
	}
	return 0;
}


int osd_close()
{
	int i;
	RGN_HANDLE handle;
	MPP_CHN_S stOverlayExChn;
	freeType_exit();
	stOverlayExChn.s32DevId = 0;
	stOverlayExChn.enModId = HI_ID_VIU;		
	for(i=0; i<MAX_OSD_SUPPORT_AREARS ; i++) {
		handle = i;
		stOverlayExChn.s32ChnId = i;
		HI_MPI_RGN_DetachFrmChn(handle, &stOverlayExChn);
		HI_MPI_RGN_Destroy(handle);
	}
	return 0;
}


static int cache_osd_time_text(const char* text, struct time_display* display, BITMAP_S *pstBitmap)
{
	int i, width, height;
	uint16_t *dst, *src;
	if(!text || !display || !pstBitmap)
		return -1;
	width = pstBitmap->u32Width;
	height = pstBitmap->u32Height;
	display->bitmap_width = width;
	display->bitmap_height = height;
	dst = (uint16_t*)display->time_bitmap;
	src = (uint16_t*)pstBitmap->pData;
	for(i=0; i<width*height; i++) {
		dst[i] = src[i];
	}
    mal_strcpy_s(display->time_string, sizeof(display->time_string), text);
	return 0;
}

static int fixoffset(int offset, int advance_x)
{
	int fixWidth;
	// different font the fixwidth need be modifided...
	fixWidth = advance_x > 20  ?  advance_x - 15 : advance_x - 10;   
	if(offset >16 && offset <19)
		return fixWidth*5;
	else if(offset >13 && offset<16)
		return fixWidth*4;
	else if(offset >10&& offset <13)
		return fixWidth*3;
	else if(offset >7 && offset <10)
		return fixWidth*2;
	else if(offset >4 && offset <7)
		return fixWidth;
	else
		return 0;
}

static int sync_osd_time_text(RGN_HANDLE handle, const char* text, struct time_display* display)
{
	char *time_string;
	BITMAP_S stBitmap;
	int offset_digits, offset = 0, total_bitmap, total_digits; 
	int i,j, bitmap_width, bitmap_height;
	int digits_width, digits_height, advance_x, advance_y;
	uint16_t *bitmap_digits, *bitmap_cache;

	time_string = display->time_string;
	digits_width = display->digits_width;
	digits_height = display->digits_height;
	bitmap_width = display->bitmap_width;
	bitmap_height = display->bitmap_height;
	if(digits_height>24)
		advance_x = digits_height/2+2;
	else
		advance_x = digits_height/2+1;
	advance_y = digits_height;
	total_bitmap = bitmap_width*bitmap_height;
	total_digits = digits_width*digits_height;
	

	while(text[offset] != 0) {
		if(text[offset] != time_string[offset]) {
			offset_digits = text[offset] - '0';
			for(i=0; i<advance_y; i++) {
				bitmap_cache = (uint16_t*)display->time_bitmap;
			    bitmap_digits = (uint16_t*)display->digits_bitmap;
				bitmap_digits += i*digits_width + (offset_digits*advance_x);
				bitmap_cache  += i*bitmap_width + (offset*advance_x);
				bitmap_cache -= fixoffset(offset, advance_x);
				for(j=0; j<advance_x; j++){
					bitmap_cache[j] = bitmap_digits[j];
				}
			}
		}
		offset++;
	}

	stBitmap.enPixelFormat = PIXEL_FORMAT_RGB_1555;
	stBitmap.u32Width = display->bitmap_width;
	stBitmap.u32Height = display->bitmap_height;
	stBitmap.pData = display->time_bitmap;
    mal_strcpy_s(time_string, sizeof(display->time_string), text);
	return HI_MPI_RGN_SetBitMap(handle, &stBitmap) == HI_SUCCESS ? 0 : -1;
}

int osd_draw_text(int channel, const char* text, unsigned int flags)
{
	int error, wlens, font_size, digits_size, fix_width=0;
	int i, line_width, line_height, offset_x = 0;
	wchar_t wtext[128] = {0};
	bitmap_info_t bitmap_info;
	u8* line_addr;
	RGN_HANDLE handle;
	wchar_t *p;
	BITMAP_S stBitmap;

	if(channel<0 || channel>=MAX_OSD_SUPPORT_AREARS)
		return -1;

	handle = channel;
	font_size = 30;
	if(flags == MAL_OSD_TEXT_FLAG_TIME &&  strlen(text)>31) 
		return -1;

	if(flags == MAL_OSD_TEXT_FLAG_TIME && time_displays.time_string[0] !=0) 
		return sync_osd_time_text(handle, text, &time_displays);

	text2bitmap_set_font_size(font_size);
	wlens = sizeof(wtext)/sizeof(wtext[0]);
	utf8_to_unicode((const uint8_t*)text, strlen(text), wtext, &wlens);

	line_height = font_size ;
	line_width = font_size  * wlens;
	digits_size = line_width * line_height;
	line_addr = (uint8_t *)malloc(digits_size);
	if(line_addr == NULL) {
		perror("digits_convert: malloc\n");
		return -1;
	}

	memset(line_addr, 0, digits_size);
	for (i = 0, p = wtext; i < wlens; i++, p++) {
		if (text2bitmap_convert_character(*p, line_addr, line_height,
				line_width, offset_x, &bitmap_info) < 0) {
			printf("text2bitmap_convert_character failed\n");
			return -1;
		}
		if(flags == MAL_OSD_TEXT_FLAG_TIME) {
			if(*p == L'-') 
				fix_width = bitmap_info.width;

			if(bitmap_info.width < fix_width) 
				bitmap_info.width = fix_width;
				
		}
//		printf("bitmap_info.width = %d\n",bitmap_info.width);
		offset_x += bitmap_info.width;
	}
	stBitmap.enPixelFormat = PIXEL_FORMAT_RGB_1555;
	stBitmap.u32Width = line_width;
	stBitmap.u32Height = font_size;
	stBitmap.pData = (uint8_t*)malloc(digits_size*2);
	if(!stBitmap.pData) {
		free(line_addr);
		return -1;
	}

	if(gray2argb1555(line_addr, digits_size, stBitmap.pData, digits_size*2) !=0) {
		free(line_addr);
		return -1;
	}

	error = HI_MPI_RGN_SetBitMap(handle, &stBitmap);
	if(flags == MAL_OSD_TEXT_FLAG_TIME) {
		cache_osd_time_text(text, &time_displays, &stBitmap);
	}
	free(line_addr);
	free(stBitmap.pData);
	return error == HI_SUCCESS ? 0 : -1;
}

int draw_osd_1s()
{
    int ret;
        char time_str[40] = {0};
        struct tm tm_time;
        time_t t = time(NULL);
        ret = (int)localtime_r(&t, &tm_time);
        if(ret == NULL){
            err_msg("localtime_r failed\n");
            return -1;
        }
        ret = (int)strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &tm_time);
        //info_msg("current time %s\n", time_str);

        if(ret == 0){
            err_msg("strftime failed\n");
            return -1;
        }
        int i = 0;
        int osd_interval;

            osd_interval = 2;

            osd_draw_text(osd_interval * i, "recording", 2);
#if 0
        for(i = 0; i < 4; i++){
            osd_draw_text(osd_interval * i, time_str, MAL_OSD_TEXT_FLAG_TIME);

            char osd[VC_OSD_LENGTH] = {0};
            status_get_venc_osd(i, osd, sizeof(osd));
            osd_draw_text(osd_interval * i + 1, osd, 2);
        }
#endif
        return 0;
}

















