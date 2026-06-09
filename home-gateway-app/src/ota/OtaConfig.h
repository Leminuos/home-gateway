#ifndef OTACONFIG_H
#define OTACONFIG_H

#include <QString>

// Đường dẫn file và URL cố định của hệ thống — không do user cấu hình.
// Thông số do user cấu hình (mqttTopic, forceUpdate, polling…) nằm trong OtaSettings.
// Thông số kết nối MQTT (host, port) nằm trong MqttSettings.
namespace OtaConfig {

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

// /data/config/ota.json — config OTA do user chỉnh, tồn tại qua OTA + reboot
inline QString configFile()
{
    return qEnvironmentVariable("OTA_CONFIG_FILE", QStringLiteral("/data/config/ota.json"));
}

// /data/config/mqtt.json — thông số kết nối MQTT broker
inline QString mqttConfigFile()
{
    return qEnvironmentVariable("MQTT_CONFIG_FILE", QStringLiteral("/data/config/mqtt.json"));
}

} // namespace OtaConfig

#endif // OTACONFIG_H
