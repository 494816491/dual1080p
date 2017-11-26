#include "configdata.h"
#define RTMP_IP_INI_KEY "rtmp/ip"

ConfigData::ConfigData(QString init_path):init_path(init_path)
{
    setting = new QSettings(init_path, QSettings::IniFormat);
}


ConfigData::~ConfigData()
{
    delete setting;
}

QString ConfigData::get_rtmp_server_ip()
{
    return this->setting->value(RTMP_IP_INI_KEY).toString();
}
#if 0

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
#endif
