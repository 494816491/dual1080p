#ifndef CONFIGDATA_H
#define CONFIGDATA_H

#define RECODER_MODE_CYCLE_VALUE 0
#define RECODER_MODE_SINGLE_VALUE 1

int status_read_ini_file();
int status_get_rtmp_file(char rtmp_addr[1000]);
int status_get_recoder_mode();
#endif // CONFIGDATA_H
