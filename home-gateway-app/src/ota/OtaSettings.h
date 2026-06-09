#ifndef OTASETTINGS_H
#define OTASETTINGS_H

#include <QJsonObject>
#include <QString>

// Config OTA lưu tại /data/config/ota.json — tồn tại qua OTA + reboot.
// Chứa các tuỳ chọn liên quan đến luồng OTA (không bao gồm thông số kết nối MQTT).
class OtaSettings
{
public:
    explicit OtaSettings(const QString &path = QString());

    void load();          // thiếu/hỏng -> giữ default
    bool save() const;    // tạo thư mục nếu cần; false nếu lỗi

    bool autoMode() const;
    void setAutoMode(bool autoMode);

    QString manifestUrl() const;
    void setManifestUrl(const QString &url);

    QString mqttTopic() const;
    void setMqttTopic(const QString &topic);

    bool forceUpdate() const;
    void setForceUpdate(bool force);

    int pollingIntervalSec() const;  // 0 = tắt polling
    void setPollingIntervalSec(int sec);

private:
    QString mPath;
    QJsonObject mRoot;
};

#endif // OTASETTINGS_H
