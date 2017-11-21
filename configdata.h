#ifndef CONFIGDATA_H
#define CONFIGDATA_H
#include <QString>
#include <QSettings>

class ConfigData
{
public:
    ConfigData(QString init_path);
    ~ConfigData();
    QString get_rtmp_server_ip();
private:
    QString init_path;
    QSettings *setting;
};

#endif // CONFIGDATA_H
