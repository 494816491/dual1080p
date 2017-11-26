#ifndef TEXT2BITMAP_H
#define TEXT2BITMAP_H

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_BYTE_SIZE	128

#ifndef _BASE_TYPES_H_
typedef unsigned char		u8;	/**< UNSIGNED 8-bit data type */
typedef unsigned short		u16;/**< UNSIGNED 16-bit data type */
typedef unsigned int		u32;	/**< UNSIGNED 32-bit data type */
typedef unsigned long long	u64; /**< UNSIGNED 64-bit data type */
typedef signed char		s8;	/**< SIGNED 8-bit data type */
typedef signed short		s16;	/**< SIGNED 16-bit data type */
typedef signed int		s32;	/**< SIGNED 32-bit data type */
#endif

typedef struct font_attribute_s {
	char type[MAX_BYTE_SIZE];
	u32 size;
	u32 outline_width;		// 0: no outline
	s32 hori_bold;			// 0: no bold at all; positive is bold, negetive is thin in horizontal
	s32 vert_bold;			// 0: no bold at all; positive is bold, negetive is thin in vertical
	u32 italic;				// 0 is normal (no italic), 100 is 100% italic
	u32 disable_anti_alias; // 0: anti-alias is enabled, 1: disable anti-alias. Default is 0.
} font_attribute_t;

typedef struct bitmap_info_s {
	int width;
	int height;
} bitmap_info_t;

typedef struct pixel_type_s {
	u8 pixel_background;
	u8 pixel_font;
	u8 pixel_outline;
	u8 reserved;
} pixel_type_t;

int text2bitmap_lib_init(pixel_type_t *type);
int text2bitmap_set_font_size(int size);
int text2bitmap_get_font_size(int *size);
int text2bitmap_set_font_attribute(const font_attribute_t *font_attr);
int text2bitmap_get_font_attribute(font_attribute_t *font_attr);
int text2bitmap_convert_character(u32 ui_char_code, u8 *bmp_addr, u16 buf_height, u16 buf_pitch, int offset_x, bitmap_info_t *bmp_info);
int text2bitmap_lib_deinit(void);


#ifdef __cplusplus
}
#endif

#endif /* TEXT2BITMAP_H */

