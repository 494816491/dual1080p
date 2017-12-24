#include <string.h>

#include <unistd.h>
#include "configdata.h"
#include "iniparser.h"
#include "debug.h"

#define INI_FILENAME "/mnt/usb/rtmp_config.ini"
#define RTMP_ADDRESS_KEY "rtmp_addr"
#define RTMP_ADDRESS_VALUE "rtmp://192.168.22.2/live/chn0"
#define RECODER_MODE_KEY "recoder_mode" //0,cycle, 1,single



static struct ini_info_st{
    dictionary *dic;
}ini_info;

static int iniparser_set_int(dictionary* dic, char *key, int val)
{
    int ret;
    char tmp[20] = {0};
    sprintf(tmp, "%d", val);
    ret = iniparser_set (dic, key, tmp);
    if(ret != 0){
        err_msg("iniparser_set  failed\n");
        return -1;
    }
    return 0;
}


static int dump_ini_file()
{
    FILE *file = fopen(INI_FILENAME, "w");
    if(file == NULL){
        err_msg("fopen failed\n");
        return -1;
    }
    iniparser_dump_ini(ini_info.dic, file);
    fclose(file);
    return 0;
}

static int status_create_default_ini()
{
    int ret;
    info_msg("before status_create_default_ini");
#if 0
    char shell_cmd[50] = {0};
    sprintf(shell_cmd, "touch %s\n", INI_FILENAME);
    ret = system(shell_cmd);
    if(ret == -1){
        err_msg("system touch /app/ini/vc.ini failed\n");
        return -1;
    }
#endif

    ini_info.dic = dictionary_new(0) ;
     //= iniparser_load(INI_FILENAME);
    if(ini_info.dic == NULL){
        err_msg("iniparser_load failed\n");
        exit(-1);
    }

    //---------------------add default
    char tmp[30] = {0};

    sprintf(tmp, "config", NULL);
    iniparser_set(ini_info.dic, tmp, RTMP_ADDRESS_VALUE);

    sprintf(tmp, "config:%s", RTMP_ADDRESS_KEY);
    iniparser_set(ini_info.dic, tmp, RTMP_ADDRESS_VALUE);

    sprintf(tmp, "config:%s", RECODER_MODE_KEY);
    iniparser_set_int(ini_info.dic, tmp, RECODER_MODE_CYCLE_VALUE);

    dump_ini_file();
    return 0;
}

int status_get_recoder_mode()
{
    char tmp[30] = {0};
    sprintf(tmp, "config:%s", RECODER_MODE_KEY);
    return iniparser_getint(ini_info.dic, tmp, 0);
}

int status_get_rtmp_addr(char rtmp_addr[1000])
{
    char key[30] = {0};
    sprintf(key, "config:%s", RTMP_ADDRESS_KEY);

    printf("key = %s\n", key);

    char *tmp = iniparser_getstring(ini_info.dic, key, NULL);
    printf("tmp = %s\n", tmp);
    strncpy(rtmp_addr, tmp, 1000);
    return 0;
}

int status_read_ini_file()
{
    int ret;
    ret = access(INI_FILENAME, F_OK);
    if(ret != 0){
        status_create_default_ini();
    }else{
        ini_info.dic = iniparser_load(INI_FILENAME);
        if(ini_info.dic == NULL){
            err_msg("iniparser_load failed\n");
            exit(-1);
        }
    }
    return 0;
}
