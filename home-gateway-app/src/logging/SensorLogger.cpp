#include "SensorLogger.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

static const char *kCsvHeader = "timestamp,temperature,humidity,lux\n";

// Lấy mẫu cho chart: tối thiểu 60s/mẫu, giữ tối đa 24h (1 mẫu/phút -> 1440).
static const qint64 kMinIntervalSec = 60;
static const int kMaxSamples = 24 * 60;

SensorLogger::SensorLogger(QObject *parent, const QString &path)
    : QObject(parent)
    , mPath(path.isEmpty()
            ? qEnvironmentVariable("SENSOR_LOG_FILE",
                                   QStringLiteral("/data/logs/sensors.csv"))
            : path)
{
}

bool SensorLogger::ensureReady()
{
    if (mReady) {
        return true;
    }

    const QString dir = QFileInfo(mPath).absolutePath();
    if (!QDir().mkpath(dir)) {
        qWarning() << "SensorLogger: cannot create dir" << dir;
        return false;
    }

    // File mới hoặc rỗng -> ghi header CSV một lần.
    const bool needHeader = !QFileInfo::exists(mPath) || QFileInfo(mPath).size() == 0;
    if (needHeader) {
        QFile f(mPath);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Append)) {
            qWarning() << "SensorLogger: cannot open" << mPath << f.errorString();
            return false;
        }
        f.write(kCsvHeader);
    }

    mReady = true;
    return true;
}

void SensorLogger::load()
{
    QFile f(mPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return; // chưa có file -> buffer rỗng
    }

    const qint64 cutoff = QDateTime::currentSecsSinceEpoch() - (qint64)kMaxSamples * kMinIntervalSec;

    mSamples.clear();
    QTextStream in(&f);
    bool firstLine = true;
    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (firstLine) {
            firstLine = false;
            if (line.startsWith(QStringLiteral("timestamp"))) {
                continue; // bỏ header
            }
        }

        const QStringList cols = line.split(',');
        if (cols.size() < 4) {
            continue;
        }

        const qint64 t = QDateTime::fromString(cols.at(0), Qt::ISODate).toSecsSinceEpoch();
        if (t <= 0 || t < cutoff) {
            continue; // bỏ mẫu quá cũ (ngoài cửa sổ 24h)
        }
        // Downsample: chỉ giữ khi cách mẫu trước >= 60s.
        if (!mSamples.isEmpty() && t - mSamples.last().t < kMinIntervalSec) {
            continue;
        }

        Sample s;
        s.t = t;
        s.temp = cols.at(1).toDouble();
        s.humi = cols.at(2).toInt();
        s.lux = cols.at(3).toInt();
        mSamples.append(s);
    }

    if (mSamples.size() > kMaxSamples) {
        mSamples.remove(0, mSamples.size() - kMaxSamples);
    }
}

bool SensorLogger::append(double temperature, int humidity, int lux)
{
    if (!ensureReady()) {
        return false;
    }

    QFile f(mPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Append)) {
        qWarning() << "SensorLogger: cannot open" << mPath << f.errorString();
        return false;
    }

    const qint64 now = QDateTime::currentSecsSinceEpoch();

    QTextStream out(&f);
    out << QDateTime::fromSecsSinceEpoch(now).toString(Qt::ISODate) << ','
        << QString::number(temperature, 'f', 1) << ','
        << humidity << ','
        << lux << '\n';
    f.close();

    // Nạp vào buffer chart theo nhịp 1/phút.
    if (mSamples.isEmpty() || now - mSamples.last().t >= kMinIntervalSec) {
        pushSample(now, temperature, humidity, lux);
    }

    return true;
}

void SensorLogger::pushSample(qint64 t, double temp, int humi, int lux)
{
    mSamples.append(Sample{t, temp, humi, lux});
    if (mSamples.size() > kMaxSamples) {
        mSamples.remove(0, mSamples.size() - kMaxSamples);
    }
    emit sampleAdded(t, temp, humi, lux);
}
