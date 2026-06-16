#ifndef SENSORLOGGER_H
#define SENSORLOGGER_H

#include <QObject>
#include <QString>
#include <QVector>

// Ghi log số liệu cảm biến (nhiệt độ, độ ẩm, ánh sáng) ra file CSV trên
// partition /data để tồn tại qua reboot/OTA. Mặc định /data/logs/sensors.csv,
// override qua env SENSOR_LOG_FILE. File được xoay vòng khi vượt ngưỡng kích
// thước để không phình vô hạn.
class SensorLogger : public QObject
{
    Q_OBJECT

public:
    struct Sample
    {
        qint64 t;       // epoch giây
        double temp;    // °C
        int humi;       // %
        int lux;        // lx
    };

    explicit SensorLogger(QObject *parent = nullptr, const QString &path = QString());

    // Đọc lại lịch sử từ CSV vào buffer RAM.
    void load();

    // Ghi 1 dòng CSV + nạp vào buffer chart.
    // Tự xoay vòng file khi vượt ngưỡng kích thước.
    bool append(double temperature, int humidity, int lux);

    const QVector<Sample> &samples() const { return mSamples; }

signals:
    void sampleAdded(qint64 t, double temp, int humi, int lux);

private:
    bool ensureReady();
    void rotateIfNeeded();
    void pushSample(qint64 t, double temp, int humi, int lux);

    QString mPath;
    bool mReady = false;
    QVector<Sample> mSamples;
};

#endif // SENSORLOGGER_H
