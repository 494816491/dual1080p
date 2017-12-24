#ifndef OSD_H
#define OSD_H

#ifdef __cplusplus
extern "C"{
#endif

int osd_open(void);
int osd_close(void);
int osd_draw_text(int channel, const char* text, unsigned int flags);

int draw_osd_1s(char *text);

#ifdef __cplusplus
}
#endif

#endif
