#ifndef MQTTSETTINGS_H
#define MQTTSETTINGS_H

#include <QJsonObject>
#include <QString>

// Thông số kết nối MQTT, lưu tại /data/config/mqtt.json.
// Tách khỏi OtaSettings để có thể thay đổi broker mà không đụng config OTA.
class MqttSettings
{
public:
    explicit MqttSettings(const QString &path = QString());

    void load();        // thiếu/hỏng -> giữ default
    bool save() const;

    QString host() const;
    void setHost(const QString &host);

    int port() const;
    void setPort(int port);

private:
    QString mPath;
    QJsonObject mRoot;
};

#endif // MQTTSETTINGS_H
