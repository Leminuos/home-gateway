#ifndef SENSORLOGGER_H
#define SENSORLOGGER_H

#include <QString>

// Ghi log số liệu cảm biến (nhiệt độ, độ ẩm, ánh sáng) ra file CSV trên
// partition /data để tồn tại qua reboot/OTA. Mặc định /data/logs/sensors.csv,
// override qua env SENSOR_LOG_FILE.
class SensorLogger
{
public:
    explicit SensorLogger(const QString &path = QString());

    bool append(double temperature, int humidity, int lux);

private:
    bool ensureReady();

    QString mPath;
    bool mReady = false;
};

#endif // SENSORLOGGER_H
