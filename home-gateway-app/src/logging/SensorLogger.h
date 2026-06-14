#ifndef SENSORLOGGER_H
#define SENSORLOGGER_H

#include <QObject>
#include <QString>
#include <QVector>

// Ghi log số liệu cảm biến (nhiệt độ, độ ẩm, ánh sáng) ra file CSV trên
// partition /data để tồn tại qua reboot/OTA. Mặc định /data/logs/sensors.csv,
// override qua env SENSOR_LOG_FILE.
//
// Ngoài việc ghi CSV (mỗi lần append), logger còn giữ một buffer trong RAM lấy
// mẫu thưa (1 mẫu/phút, cap 24h gần nhất) làm nguồn dữ liệu cho ChartDashboard:
// load() đọc lại lịch sử từ CSV, samples() trả buffer, sampleAdded() báo có mẫu
// mới được đưa vào buffer.
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

    // Đọc lại lịch sử từ CSV vào buffer RAM (downsample >=60s, giữ <=24h).
    void load();

    // Ghi 1 dòng CSV; đồng thời nạp vào buffer chart nếu đã qua >=60s kể từ mẫu
    // trước (kèm phát sampleAdded).
    bool append(double temperature, int humidity, int lux);

    const QVector<Sample> &samples() const { return mSamples; }

signals:
    void sampleAdded(qint64 t, double temp, int humi, int lux);

private:
    bool ensureReady();
    void pushSample(qint64 t, double temp, int humi, int lux);

    QString mPath;
    bool mReady = false;
    QVector<Sample> mSamples;
};

#endif // SENSORLOGGER_H
