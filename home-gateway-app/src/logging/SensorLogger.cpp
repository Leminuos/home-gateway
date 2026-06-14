#include "SensorLogger.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

static const char *kCsvHeader = "timestamp,temperature,humidity,lux\n";

SensorLogger::SensorLogger(const QString &path)
    : mPath(path.isEmpty()
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

    QTextStream out(&f);
    out << QDateTime::currentDateTime().toString(Qt::ISODate) << ','
        << QString::number(temperature, 'f', 1) << ','
        << humidity << ','
        << lux << '\n';

    return true;
}
