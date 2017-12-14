#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "debug.h"

#include "disk_manage.h"


#define SQL_CLAUSE_LEN 1024
#define MAX_FILE_NAME_LEN 1024
#define VIDEO_PRE_LEN 12
#define DISK_NUM 1
#define STORAGE_INDEX 0


#define DB_PATH_NAME "/mnt/usb/index.sqlite"

struct storage_st{
    bool is_disk_exist;
    int picture_capability;
    int video_capability;
    int remain_video_disk;
    int remain_pic_disk;
};

struct db_status_st{
    sqlite3 *db;
    pthread_mutex_t mutex;
};

struct watch_disk_status_st{
    struct storage_st storage[DISK_NUM];
    pthread_t format_thread_id[DISK_NUM];
};

static struct watch_disk_status_st watch_disk_status;
static struct db_status_st db_status = { NULL, PTHREAD_MUTEX_INITIALIZER};



static int update_sum_size(char *item, int num)
{
    sqlite3_stmt *stmt;
    char *mess;
    int ret;

    char set_sum_size[SQL_CLAUSE_LEN] = {0};
    sprintf(set_sum_size, "update sum_table set total_size = %d where item = '%s';", num, item);
    //info_msg("%s\n", set_sum_size);

     ret = sqlite3_exec(db_status.db, set_sum_size, NULL, NULL, &mess);
     if(ret != SQLITE_OK){
         err_msg("sqlite3_exec failed\n%s\n", mess);
         sqlite3_free(mess);
         return -1;
     }
     return 0;
}

static int check_chn_update_size(int chn_num)
{
    int ret;
    char find_file[SQL_CLAUSE_LEN] = {0};
    //sprintf(find_file, "select * from chn%d  where size is null;", chn_num);
    //sprintf(find_file, "select * from chn%d;", chn_num);
    sprintf(find_file, "select * from chn%d where end_time is null;", chn_num);

    sqlite3_stmt *stmt;
    ret = sqlite3_prepare_v2(db_status.db, find_file, -1, &stmt, NULL);
    if(ret != SQLITE_OK){
        err_msg("%s\n",sqlite3_errmsg(db_status.db));
        ret = -1;
    }

    ret = sqlite3_step(stmt);
    while(ret == SQLITE_ROW){
        char file_name[MAX_FILE_NAME_LEN] = {0};
        const char *rm_file = (const char *)sqlite3_column_text(stmt, 0);
        sprintf(file_name, "%s/%s", VIDEO_SAVE_PATH, rm_file);

        if(access(file_name, F_OK)){
            char delete_clause[SQL_CLAUSE_LEN] = {0};
            sprintf(delete_clause, "delete from chn%d where file_name = '%s';", chn_num,  file_name + VIDEO_PRE_LEN);
            info_msg("%s\n", delete_clause);
            char *mess;
            ret = sqlite3_exec(db_status.db, delete_clause, NULL, NULL, &mess);
            if(ret != SQLITE_OK){
                err_msg("%s", mess);
                sqlite3_free(mess);
                ret = -1;
            }
            //}else if(sqlite3_column_int(stmt, 5) == 0){
            //找到没有被写end_time 的条目，更新其大小，并且写入end_time = start_time,在查询回放文件的时候需要使用到end_time 和 start_time之间的差值来判断文件是否存在文件尾
        }else if(sqlite3_column_int(stmt, 3) == 0){
            info_msg("step, %s\n", file_name);
            struct stat file_stat;
            ret = stat(file_name, &file_stat);
            if(ret == 0){
                char register_clause[SQL_CLAUSE_LEN] = {0};
                //sprintf(register_clause, "update chn%d set size = %ld where file_name = '%s';", chn_num, file_stat.st_blocks, file_name + VIDEO_PRE_LEN);
                sprintf(register_clause, "update chn%d set size = %ld,end_time = start_time where file_name = '%s';", chn_num, file_stat.st_blocks, file_name + VIDEO_PRE_LEN);

                char *mess;
                ret = sqlite3_exec(db_status.db, register_clause, NULL, NULL, &mess);
                if(ret != SQLITE_OK){
                    err_msg("%s", mess);
                    sqlite3_free(mess);
                    ret = -1;
                }
            }
        }
        ret = sqlite3_step(stmt);
    }
    ret = sqlite3_finalize(stmt);
    if(ret != SQLITE_OK){
        err_msg("%s, %d\n",sqlite3_errmsg(db_status.db), ret);
        ret = -1;
    }
    return 0;
}

static int update_chn_sum_size(int chn)
{
    int ret;
    sqlite3_stmt *stmt;
    int video_sum_size, pic_sum_size;
    //video size
    char chn_item[200] = {0};
    char get_size_sum[SQL_CLAUSE_LEN] = {0};
     sprintf(get_size_sum, "select sum(size) from chn%d;", chn);
    ret = sqlite3_prepare_v2(db_status.db, get_size_sum, -1, &stmt, NULL);
    if(ret != SQLITE_OK){
        err_msg("%s\n",sqlite3_errmsg(db_status.db));
        ret = -1;
    }
    ret = sqlite3_step(stmt);
    if(ret != SQLITE_ROW){
        err_msg("get file chn%d total size failed, %s\n", chn, sqlite3_errmsg(db_status.db));
        return -1;
    }
    video_sum_size = sqlite3_column_int(stmt, 0);
    sprintf(chn_item, "chn%d_sum_size", chn);
    update_sum_size(chn_item, video_sum_size);
    ret = sqlite3_finalize(stmt);
    if(ret != SQLITE_OK){
        err_msg("%s, %d\n",sqlite3_errmsg(db_status.db), ret);
        ret = -1;
    }
    //pic size
    memset(get_size_sum, 0, sizeof(get_size_sum));
     sprintf(get_size_sum, "select sum(size) from chn%d_pic;", chn);
    ret = sqlite3_prepare_v2(db_status.db, get_size_sum, -1, &stmt, NULL);
    if(ret != SQLITE_OK){
        err_msg("%s\n",sqlite3_errmsg(db_status.db));
        ret = -1;
    }
    ret = sqlite3_step(stmt);
    if(ret != SQLITE_ROW){
        err_msg("get file chn%d total size failed, %s\n", chn, sqlite3_errmsg(db_status.db));
        return -1;
    }
    pic_sum_size = sqlite3_column_int(stmt, 0);
    memset(chn_item, 0, sizeof(chn_item));
    sprintf(chn_item, "chn%d_pic_sum_size", chn);
    update_sum_size(chn_item, pic_sum_size);
    ret = sqlite3_finalize(stmt);
    if(ret != SQLITE_OK){
        err_msg("%s, %d\n",sqlite3_errmsg(db_status.db), ret);
        ret = -1;
    }
    return 0;
}
int check_database_size()
{
    if(!db_status.db){
        return 0;
    }
    int i = 0;
    for(i = 0; i < 8; i++){
        check_chn_update_size(i);
        update_chn_sum_size(i);
    }
    return 0;
}

static int initialize_watch_disks_status()
{
    int ret;
    char shell_cmd[400];
    int i;

    for(i = 0; i < DISK_NUM; i++){
        //sd card
        memset(shell_cmd, 0, sizeof(shell_cmd));

        //sprintf(shell_cmd,"mount | grep '/mnt/sd%d'", i);
        sprintf(shell_cmd,"mount | grep '%s'", VIDEO_SAVE_PATH);
        ret = system(shell_cmd);
        //info_msg("system ----- ret = ")
        if(ret == 0){
            int total_disk;
            memset(&shell_cmd, 0, sizeof(shell_cmd));
            watch_disk_status.storage[i].is_disk_exist  = true;

            //sprintf(shell_cmd, "df | grep /mnt/sd%d | awk '{print $2}'", i);
            sprintf(shell_cmd,"df | grep '%s'| awk '{print $2}'", VIDEO_SAVE_PATH);
            total_disk = shell_cmd_get_int(shell_cmd);

            watch_disk_status.storage[i].video_capability = total_disk * 9 / 10 * 0.95;
            watch_disk_status.storage[i].picture_capability = total_disk / 10 * 0.95;

            watch_disk_status.storage[i].remain_video_disk = watch_disk_status.storage[i].video_capability * 0.03;
            watch_disk_status.storage[i].remain_pic_disk = watch_disk_status.storage[i].picture_capability * 0.03;
            info_msg("remain video disk = %d, remain pic disk = %d\n", watch_disk_status.storage[i].remain_video_disk, watch_disk_status.storage[i].remain_pic_disk);
        }else{
            watch_disk_status.storage[i].is_disk_exist  = false;
            watch_disk_status.storage[i].video_capability = 0;
            watch_disk_status.storage[i].picture_capability = 0;
        }
    }

    //初始化数据库
    if(watch_disk_status.storage[STORAGE_INDEX].is_disk_exist){
        if(db_status.db == NULL){
            if(open_init_db() != 0){
                err_msg("open_init_db failed\n");
                return -1;
            }
            //查找数据库中没有记录大小的文件，添加大小记录,开机做一次检查
            check_database_size();
            info_msg("after check_database_size\n");
        }
    }

    return 0;
}

int disk_manage_init()
{
    initialize_watch_disks_status();
    return 0;
}



static int get_video_used_size(sqlite3 *db, pthread_mutex_t *mutex)
{
    int ret;
    if(db == NULL){
        return 0;
    }
    sqlite3_stmt *stmt;
    int used_size = -1;
    //char get_size_sum[SQL_CLAUSE_LEN] = "select sum(total_size) from sum_table where item like 'chn__sum_size';";
    char get_size_sum[SQL_CLAUSE_LEN] = {0};
    sprintf(get_size_sum, "select sum(total_size) from sum_table where item like 'chn__sum_size';");
    ret = sqlite3_prepare_v2(db, get_size_sum, -1, &stmt, NULL);
    if(ret != SQLITE_OK){
        err_msg("%s\n",sqlite3_errmsg(db));
        used_size = -1;
    }

    ret = sqlite3_step(stmt);
    if(ret != SQLITE_ROW){
        err_msg("%s, %d\n",sqlite3_errmsg(db), ret);
        used_size = -1;
    }
    used_size= sqlite3_column_int(stmt, 0) / 2;

    ret = sqlite3_finalize(stmt);
    if(ret != SQLITE_OK){
        err_msg("%s, %d\n",sqlite3_errmsg(db), ret);
        used_size = -1;
    }

    return used_size;
}

static int get_picture_used_size(sqlite3 *db, pthread_mutex_t *mutex)
{
    if(db  == NULL){
        return 0;
    }
    int ret;
    sqlite3_stmt *stmt;
    int used_size = -1;
    char get_size_sum[SQL_CLAUSE_LEN] = "select sum(total_size) from sum_table where item like 'chn__pic_sum_size';";
    ret = sqlite3_prepare_v2(db, get_size_sum, -1, &stmt, NULL);
    if(ret != SQLITE_OK){
        err_msg("%s\n",sqlite3_errmsg(db));
        used_size = -1;
    }

    ret = sqlite3_step(stmt);
    if(ret != SQLITE_ROW){
        err_msg("%s, %d\n",sqlite3_errmsg(db), ret);
        used_size = -1;
    }
    used_size= sqlite3_column_int(stmt, 0) / 2;

    ret = sqlite3_finalize(stmt);
    if(ret != SQLITE_OK){
        err_msg("%s, %d\n",sqlite3_errmsg(db), ret);
        used_size = -1;
    }

    return used_size;
}

int watch_find_delete_pic()
{
    //1.avaiable disk
    if(!db_status.db){
        return 0;
    }
    while(1){
        int avaiable_disk;
        //info_msg("before get_picture_used_size\n");
        int used_size = get_picture_used_size(db_status.db, &db_status.mutex);
        avaiable_disk = watch_disk_status.storage[STORAGE_INDEX].picture_capability - used_size;
        //info_msg(" current pic used size = %dKB avaiable pic disk = %dKB\n", used_size , avaiable_disk);
        //info_msg("watch_find_delete_pic while--------------\n");
        //if(avaiable_disk < status_get_pic_remain_disk()){
        if(avaiable_disk < watch_disk_status.storage[STORAGE_INDEX].remain_pic_disk){
            //info_msg("before delete_old_snap_file\n");
            //delete_old_snap_file(db_status.db, &db_status.mutex);
        }else{
            break;
        }
    }
    return 0;
}

static int get_free_video_disk()
{
    int avilable;

    int used_size = get_video_used_size(db_status.db, &db_status.mutex);

    avilable = watch_disk_status.storage[STORAGE_INDEX].video_capability - used_size;
    //info_msg("used video disk = %dKB, free video disk = %dKB\n", used_size, avilable);
    return avilable;
}

int should_delete_old_file()
{
    int free_disk;
    free_disk = get_free_video_disk();
    if(free_disk <= watch_disk_status.storage[STORAGE_INDEX].remain_video_disk){
        return true;
    }else{
        return false;
    }
}



int open_init_db()
{
    int ret;
    char *mess = NULL;
    int i;

    //1.open db file
    char db_file[MAX_FILE_NAME_LEN] = {0};
    sprintf(db_file,  DB_PATH_NAME);
    info_msg("==================open_init_db================\n", db_file);

    ret = sqlite3_open(db_file, &db_status.db);
    if(ret != SQLITE_OK){
        sqlite3_errmsg(db_status.db);
        return -1;
    }

    //2. to find whether is my db
    sqlite3_stmt *stmt;
    char a[] = "select count(*) from sqlite_master where type='table' and name='chn7';";

    ret = sqlite3_prepare_v2(db_status.db, a, sizeof(a), &stmt, NULL);
    if(ret != SQLITE_OK){
        sqlite3_errmsg(db_status.db);
        return -1;
    }

    ret = sqlite3_step(stmt);
    if(ret != SQLITE_ROW){
        err_msg("%s\n",sqlite3_errmsg(db_status.db));
        return -1;
    }
    //find is my db
    ret = sqlite3_column_int(stmt, 0);
    if(ret == 1){
        ret = sqlite3_finalize(stmt);
        if(ret != SQLITE_OK){
            err_msg("%s\n",sqlite3_errmsg(db_status.db));
            return -1;
        }
    }else{
        ret = sqlite3_finalize(stmt);
        if(ret != SQLITE_OK){
            err_msg("%s\n",sqlite3_errmsg(db_status.db));
            return -1;
        }
        //3. to initialize tables
        int i;
        char add_table[SQL_CLAUSE_LEN];
        for(i = 0; i < 8; i++){
            memset(add_table, 0, sizeof(add_table));
            sprintf(add_table, "CREATE TABLE chn%d (file_name text, start_time integer, register_time integer, end_time integer, is_dirty integer, size integer);CREATE TABLE chn%d_pic (file_name text, snap_time integer, snap_reason text, size interger);", i, i);
            char *mess;

            ret = sqlite3_exec(db_status.db, add_table, NULL, NULL, &mess);
            if(ret != SQLITE_OK){
                err_msg("%s", mess);
                sqlite3_free(mess);
                return -1;
            }
        }
    }
    //添加sum_table表
    char find_sum_table[] = "select count(*) from sqlite_master where type='table' and name='sum_table';";

    ret = sqlite3_prepare_v2(db_status.db, find_sum_table, sizeof(find_sum_table), &stmt, NULL);
    if(ret != SQLITE_OK){
        sqlite3_errmsg(db_status.db);
        return -1;
    }

    ret = sqlite3_step(stmt);
    if(ret != SQLITE_ROW){
        err_msg("%s\n",sqlite3_errmsg(db_status.db));
        return -1;
    }
    ret = sqlite3_column_int(stmt, 0);
    if(ret == 1){
        ret = sqlite3_finalize(stmt);
        if(ret != SQLITE_OK){
            err_msg("%s\n",sqlite3_errmsg(db_status.db));
            return -1;
        }
    }else{
        ret = sqlite3_finalize(stmt);
        if(ret != SQLITE_OK){
            err_msg("%s\n",sqlite3_errmsg(db_status.db));
            return -1;
        }
        char add_sum_table[SQL_CLAUSE_LEN];
        memset(add_sum_table, 0, sizeof(add_sum_table));
        sprintf(add_sum_table, "CREATE TABLE sum_table(item text, total_size integer);");
        ret = sqlite3_exec(db_status.db, add_sum_table, NULL, NULL, &mess);
        if(ret != SQLITE_OK){
            err_msg("%s\n", mess);
            sqlite3_free(mess);
            return -1;
        }
        //插入sum表
        for(i = 0; i < 8; i++){
            char item_row[SQL_CLAUSE_LEN] = {0};
            char *mess;
            sprintf(item_row, "insert into sum_table(item) values('chn%d_sum_size');", i);
            ret = sqlite3_exec(db_status.db, item_row, NULL, NULL, &mess);
            if(ret != SQLITE_OK){
                err_msg("%s\n", mess);
                sqlite3_free(mess);
                return -1;
            }

            memset(item_row, 0, sizeof(item_row));
            sprintf(item_row, "insert into sum_table(item) values('chn%d_pic_sum_size');", i);
            ret = sqlite3_exec(db_status.db, item_row, NULL, NULL, &mess);
            if(ret != SQLITE_OK){
                err_msg("%s\n", mess);
                sqlite3_free(mess);
                return -1;
            }
        }
        //创建 trigger
        for(i = 0; i < 8; i++){
            //video update trigger
            char create_trigger[SQL_CLAUSE_LEN] = {0};
#if 0
            sprintf(create_trigger, "create trigger video%d_update_trigger after update of size on chn%d begin "
                                    "update sum_table set total_size = ifnull(total_size,0) - ifnull(old.size, 0) + if(new.size, 0) "
                                    "where item = 'chn%d_sum_size'; end;", i, i, i);
#endif
            sprintf(create_trigger, "create trigger video%d_update_trigger after update of size on chn%d begin "
                                    "update sum_table set total_size = ifnull(total_size,0) - ifnull(old.size, 0) + ifnull(new.size, 0) "
                                    "where item = 'chn%d_sum_size'; end;", i, i, i);
            //info_msg("create trigger = %s;\n",create_trigger);
            ret = sqlite3_exec(db_status.db, create_trigger, NULL, NULL, &mess);
            if(ret != SQLITE_OK){
                err_msg("%s\n", mess);
                sqlite3_free(mess);
                return -1;
            }
            //video delete trigger
            memset(create_trigger, 0, sizeof(create_trigger));
            sprintf(create_trigger, "create trigger video%d_delete_trigger after delete on chn%d begin "
                                    "update sum_table set total_size = ifnull(total_size,0) - ifnull(old.size, 0) "
                                    "where item = 'chn%d_sum_size'; end;", i, i, i);
            ret = sqlite3_exec(db_status.db, create_trigger, NULL, NULL, &mess);
            if(ret != SQLITE_OK){
                err_msg("%s\n", mess);
                sqlite3_free(mess);
                return -1;
            }
            //pic insert trigger
            memset(create_trigger, 0, sizeof(create_trigger));
            sprintf(create_trigger, "create trigger pic%d_insert_trigger after insert on chn%d_pic begin "
                                    "update sum_table set total_size = ifnull(total_size,0) + ifnull(new.size, 0) "
                                    "where item = 'chn%d_pic_sum_size'; end;", i, i, i);
            ret = sqlite3_exec(db_status.db, create_trigger, NULL, NULL, &mess);
            if(ret != SQLITE_OK){
                err_msg("%s\n", mess);
                sqlite3_free(mess);
                return -1;
            }
            //pic delete trigger
            memset(create_trigger, 0, sizeof(create_trigger));
            sprintf(create_trigger, "create trigger pic%d_delete_trigger after delete on chn%d_pic begin "
                                    "update sum_table set total_size = ifnull(total_size,0) - ifnull(old.size, 0) "
                                    "where item = 'chn%d_pic_sum_size'; end;", i, i, i);
            ret = sqlite3_exec(db_status.db, create_trigger, NULL, NULL, &mess);
            if(ret != SQLITE_OK){
                err_msg("%s\n", mess);
                sqlite3_free(mess);
                return -1;
            }
        }
    }
    return 0;
}



static int __delete_pre_video_file(sqlite3 *db, pthread_mutex_t *mutex)
{
    char find_rm_file[SQL_CLAUSE_LEN * 2] ="select * from(select rowid,*  from chn0  order by rowid limit 1) union all select * from (select rowid,*  from chn1  order by rowid limit 1) union all select * from(select rowid,*  from chn2  order by rowid limit 1) union all select * from(select rowid,*  from chn3  order by rowid limit 1) union all select * from(select rowid,*  from chn4  order by rowid limit 1) union all select * from (select rowid,*  from chn5  order by rowid limit 1) union all select * from (select rowid,*  from chn6  order by rowid limit 1) union all select * from (select rowid,*  from chn7  order by rowid limit 1) order by start_time limit 1;";

    sqlite3_stmt *stmt;
    int ret;

    pthread_mutex_lock(mutex);
    ret = sqlite3_prepare_v2(db, find_rm_file, -1, &stmt, NULL);
    if(ret != SQLITE_OK){
        err_msg("%s\n",sqlite3_errmsg(db));
        ret = -1;
        goto ERR_HANDLE;
    }

    //info_msg("before sqlite3_step2 \n");
    ret = sqlite3_step(stmt);
    if(ret != SQLITE_ROW){
        err_msg("%s, %d\n",sqlite3_errmsg(db), ret);
        ret = -1;
        goto ERR_HANDLE;
    }
    //info_msg("before sqlite3_column_int\n");
    int64_t rowid = sqlite3_column_int64(stmt, 0);

    const char *rm_file = (const char *)sqlite3_column_text(stmt, 1);
    char *chn_char = strchr(rm_file, '.');
    if(chn_char == NULL){
        err_msg("strchr failed\n");
        ret = -1;
        goto ERR_HANDLE;
    }
    //int chn_num = *(chn_char--) - '0';
    int chn_num = *(--chn_char) - '0';
    //printf("chn_char = %s, chn_num = %d, \n", chn_char, chn_num);

    info_msg("rm_file = %s\n", rm_file);
    char file_name[MAX_FILE_NAME_LEN] = {0};
    sprintf(file_name, "%s/%s", VIDEO_SAVE_PATH, rm_file);

    ret = remove(file_name);
    if(ret != 0){
        err_msg("remove file failed\n");
    }

    char rm_file_cache[MAX_FILE_NAME_LEN] = {0};
    strcpy(rm_file_cache, rm_file);


    ret = sqlite3_finalize(stmt);
    if(ret != SQLITE_OK){
        err_msg("%s, %d\n",sqlite3_errmsg(db), ret);
        ret = -1;
        goto ERR_HANDLE;
    }

    char rm_file_index[SQL_CLAUSE_LEN] = {0};
    char *err;
    sprintf(rm_file_index, "delete from chn%d where rowid =%lld;",chn_num,  rowid);
    //info_msg("sql rm_file_index = %s\n", rm_file_index);
    ret = sqlite3_exec(db, rm_file_index, NULL, NULL,  &err);
    if(ret != SQLITE_OK){
        err_msg("%s", err);
        sqlite3_free(err);
        ret = -1;
        goto ERR_HANDLE;
    }

    ret = 0;
ERR_HANDLE:
    pthread_mutex_unlock(mutex);
    return ret;

}

int delete_pre_video_file()
{
    __delete_pre_video_file(db_status.db, &db_status.mutex);
    return 0;
}

static int add_file_row(char *file_name, int has_voice, int chn_num, int start_time)
{
    sqlite3_stmt *stmt;
    int ret;
    char add_file_clause[SQL_CLAUSE_LEN] = {0};

    // remove /mnt/video0/ 12
    sprintf(add_file_clause, "insert into chn%d (file_name, start_time) values('%s', %d)", chn_num, file_name, start_time);
    //info_msg("%s\n", add_file_clause);

    pthread_mutex_lock(&db_status.mutex);

    ret = sqlite3_prepare_v2(db_status.db, add_file_clause, -1, &stmt, NULL);
    if(ret != SQLITE_OK){
        err_msg("%s\n",sqlite3_errmsg(db_status.db));
        ret = -1;
        goto ERR_HANDLE;
    }

    ret = sqlite3_step(stmt);
    if(ret != SQLITE_DONE){
        err_msg("%s, %d\n",sqlite3_errmsg(db_status.db), ret);
        ret = -1;
        goto ERR_HANDLE;
    }

    ret = sqlite3_finalize(stmt);
    if(ret != SQLITE_OK){
        err_msg("%s, %d\n",sqlite3_errmsg(db_status.db), ret);
        ret = -1;
        goto ERR_HANDLE;
    }
    ret = 0;
ERR_HANDLE:
    pthread_mutex_unlock(&db_status.mutex);
    return ret;
}

int create_file_index(int chn_num, char *file_name, int has_voice)
{
    //1.open and init db
    int start_time = time(NULL);
    //info_msg("time compare start_time = %d, time_t = %d\n", start_time, time(NULL));
    //2.to create file row
    // remove /mnt/video0/ 12
    add_file_row(file_name + VIDEO_PRE_LEN, has_voice, chn_num, start_time);
    return 0;
}


int register_file_index_finished(int chn_num, char *file_name)
{
    if(db_status.db == NULL){
        return -1;
    }
    int finish_time = time(NULL);
    //info_msg("register_file_index_finished time compare finish_time = %d, time_t = %d\n", finish_time, time(NULL));

    int ret;
    char register_clause[SQL_CLAUSE_LEN] = {0};
    struct stat file_stat;
    ret = stat(file_name, &file_stat);
    if(ret != 0){
        err_msg("stat failed %s\n", file_name);
    }

    sprintf(register_clause, "update chn%d set end_time = %d, size = %ld where file_name = '%s';", chn_num, finish_time, file_stat.st_blocks, file_name + VIDEO_PRE_LEN);
    //info_msg("----------register_clause size = %d,  %s\n", strlen(register_clause), register_clause);
    //info_msg("%s\n", register_clause);
    char *mess = NULL;
    pthread_mutex_lock(&db_status.mutex);
    ret = sqlite3_exec(db_status.db, register_clause, NULL, NULL, &mess);
    if(ret != SQLITE_OK){
        err_msg("%s", mess);
        sqlite3_free(mess);
        ret = -1;
        goto ERR_HANDLE;
    }
    ret = 0;
ERR_HANDLE:
    pthread_mutex_unlock(&db_status.mutex);
    return ret;
}

int register_file_size(int chn_num, char *file_name)
{
    char register_clause[SQL_CLAUSE_LEN] = {0};

    int ret;
    struct stat file_stat;
    ret = stat(file_name, &file_stat);
    if(ret != 0){
        err_msg("register_file_size failed, no such file\n");
        return -1;
    }
    sprintf(register_clause, "update chn%d set size = %ld where file_name = '%s';", chn_num,  file_stat.st_blocks, file_name + VIDEO_PRE_LEN);
    pthread_mutex_lock(&db_status.mutex);
    char *mess = NULL;
    ret = sqlite3_exec(db_status.db, register_clause, NULL, NULL, &mess);
    if(ret != SQLITE_OK){
        err_msg("%s", mess);
        sqlite3_free(mess);
        ret = -1;
    }
    ret = 0;
    pthread_mutex_unlock(&db_status.mutex);
    return 0;
}

int register_file_piece(int chn_num, char *file_name)
{
    int current_time = time(NULL);
    int ret;
    if(db_status.db == NULL){
        return -1;
    }
    char register_clause[SQL_CLAUSE_LEN] = {0};
    sprintf(register_clause, "update chn%d set register_time = %d where file_name = '%s';", chn_num, current_time, file_name + VIDEO_PRE_LEN);
    //info_msg("register_clause = %s\n", register_clause);
    char *mess = NULL;
    ret = sqlite3_exec(db_status.db, register_clause, NULL, NULL, &mess);
    if(ret != SQLITE_OK){
        err_msg("%s", mess);
        sqlite3_free(mess);
        return -1;
    }
    return 0;
}

int shell_cmd_get_int(char *cmd)
{
    FILE *out_file;
    char out_data[20] = {0};
    out_file = popen(cmd, "r");
    fread(out_data, sizeof(out_data), 1, out_file);
    pclose(out_file);
    return atoi(out_data);
}

int is_disk_is_exit()
{
    return watch_disk_status.storage[STORAGE_INDEX].is_disk_exist;
}


