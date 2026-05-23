#ifndef FIRMWAREMANIFEST_H
#define FIRMWAREMANIFEST_H

#include <QByteArray>
#include <QString>

// Manifest firmware host công bố. Cùng payload JSON cho HTTP /manifest.json
// (manual) lẫn MQTT retained ota/latest (auto).
struct FirmwareManifest
{
    QString version;   // bắt buộc — vd "0.2.0"
    QString url;       // bắt buộc — URL tuyệt đối tới .swu
    qint64  size = 0;  // optional
    QString sha256;    // optional — verify sau khi tải nếu có

    bool isValid() const { return !version.isEmpty() && !url.isEmpty(); }

    // Manifest rỗng (isValid()==false) nếu JSON lỗi.
    static FirmwareManifest fromJson(const QByteArray &json);

    // Semver-lite: >0 nếu a mới hơn b. Bỏ tiền tố 'v'; phần không phải số = 0.
    static int compareVersion(const QString &a, const QString &b);
};

#endif // FIRMWAREMANIFEST_H
