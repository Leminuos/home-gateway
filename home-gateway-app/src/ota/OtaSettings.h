#ifndef OTASETTINGS_H
#define OTASETTINGS_H

#include <QJsonObject>
#include <QString>

// Config OTA do user chỉnh (auto/manual), lưu JSON ở /data — giữ qua OTA + reboot.
// JSON object để dễ thêm field sau. Lỗi I/O xử lý mềm (default / trả false).
class OtaSettings
{
public:
    explicit OtaSettings(const QString &path = QString());

    void load();          // thiếu/hỏng -> giữ default
    bool save() const;    // tạo thư mục nếu cần; false nếu lỗi

    bool autoMode() const;          // default: false (Manual)
    void setAutoMode(bool autoMode);

private:
    QString mPath;
    QJsonObject mRoot;
};

#endif // OTASETTINGS_H
