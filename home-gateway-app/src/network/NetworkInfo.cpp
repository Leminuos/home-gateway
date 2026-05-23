#include "NetworkInfo.h"

#include <QFile>
#include <QTextStream>

QString NetworkInfo::readMacAddress(const QString &iface)
{
    QFile file("/sys/class/net/" + iface + "/address");
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        return QString();
    }
    const QString mac = QTextStream(&file).readAll().trimmed().toUpper();
    file.close();
    return mac;
}
