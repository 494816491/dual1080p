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

