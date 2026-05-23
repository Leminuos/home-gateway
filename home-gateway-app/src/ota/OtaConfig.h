#ifndef OTACONFIG_H
#define OTACONFIG_H

#include <QString>

// Cấu hình OTA — đọc từ env (set trong home-dashboard.service), có default để dev.
namespace OtaConfig {

// manifest.json — dùng cho mode manual (HTTP GET)
inline QString manifestUrl()
{
    return qEnvironmentVariable("OTA_MANIFEST_URL",
                                QStringLiteral("http://192.168.137.1:8000/manifest.json"));
}

// topic retained host publish version mới — dùng cho mode auto
inline QString mqttTopic()
{
    return qEnvironmentVariable("OTA_MQTT_TOPIC", QStringLiteral("ota/latest"));
}

// webserver của SWUpdate daemon trên BBB — app POST .swu vào đây để flash
inline QString swupdateUploadUrl()
{
    return qEnvironmentVariable("OTA_SWUPDATE_URL",
                                QStringLiteral("http://127.0.0.1:8080/upload"));
}

// socket SWUpdate phát tiến độ install
inline QString swupdateProgressSocket()
{
    return qEnvironmentVariable("OTA_SWUPDATE_PROGRESS",
                                QStringLiteral("/tmp/swupdateprog"));
}

// /etc/sw-versions (meta-ota ghi) — nguồn version hiện tại
inline QString currentVersionFile()
{
    return qEnvironmentVariable("OTA_VERSION_FILE", QStringLiteral("/etc/sw-versions"));
}

inline QString downloadPath()
{
    return qEnvironmentVariable("OTA_DOWNLOAD_PATH", QStringLiteral("/tmp/home-gateway-update.swu"));
}

// Config user (auto/manual) — đặt ở /data để giữ nguyên qua OTA + reboot.
inline QString configFile()
{
    return qEnvironmentVariable("OTA_CONFIG_FILE", QStringLiteral("/data/ota/config.json"));
}

// TEST mode: bỏ qua so sánh version — coi mọi manifest hợp lệ là "có bản mới".
// Bật bằng env OTA_FORCE_UPDATE=1 để test lặp lại cùng 1 .swu không cần bump
// version; bỏ env (hoặc =0) là về hành vi bình thường.
inline bool forceUpdate()
{
    const QString v = qEnvironmentVariable("OTA_FORCE_UPDATE").trimmed().toLower();
    return v == QStringLiteral("1") || v == QStringLiteral("true") || v == QStringLiteral("yes");
}

} // namespace OtaConfig

#endif // OTACONFIG_H
