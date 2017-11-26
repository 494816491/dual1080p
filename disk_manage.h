#ifndef DISK_MANAGE_H
#define DISK_MANAGE_H
#include <stdbool.h>
#include <pthread.h>
#include "sqlite3.h"
#include "stdint.h"

int disk_manage_init();
int initialize_watch_disks_status();
int open_init_db();
int check_database_size();
int venc_control_param_to_monitor(int chn);
int get_free_video_disk();
int delete_pre_video_file(sqlite3 *db, pthread_mutex_t *mutex);

struct storage_st{
    bool is_disk_exist;
    int picture_capability;
    int video_capability;
    int remain_video_disk;
    int remain_pic_disk;
};

struct watch_disk_status_st{
    bool is_storage_exist;
    int current_store_media; //0 hard_disk, 1 sd card
    int current_disk_index; //
    struct storage_st storage[2];
    pthread_t format_thread_id[2];
};
extern struct watch_disk_status_st watch_disk_status;




#endif // DISK_MANAGE_H
