#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <QJsonObject>
#include <QString>

// Cấu hình của home-dashboard (độ sáng màn hình,…), lưu tại
// /data/config/setting.json. Tồn tại qua OTA + reboot như các config khác.
class AppSettings
{
public:
    explicit AppSettings(const QString &path = QString());

    void load();        // thiếu/hỏng -> giữ default
    bool save() const;

    // Độ sáng màn hình theo thang slider 0..100.
    int brightness() const;
    void setBrightness(int percent);

private:
    QString mPath;
    QJsonObject mRoot;
};

#endif // APPSETTINGS_H
