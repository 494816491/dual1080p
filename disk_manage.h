#ifndef DISK_MANAGE_H
#define DISK_MANAGE_H
#include <stdbool.h>
#include <pthread.h>
#include "sqlite3.h"
#include "stdint.h"

#define VIDEO_SAVE_PATH "/mnt/usb"

int is_disk_is_exit();

int disk_manage_init();
int open_init_db();
int check_database_size();
int venc_control_param_to_monitor(int chn);
int delete_pre_video_file();

int should_delete_old_file();





#endif // DISK_MANAGE_H
