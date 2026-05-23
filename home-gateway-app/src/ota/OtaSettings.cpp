#include "OtaSettings.h"
#include "OtaConfig.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>

OtaSettings::OtaSettings(const QString &path)
    : mPath(path.isEmpty() ? OtaConfig::configFile() : path)
{
}

void OtaSettings::load()
{
    QFile f(mPath);
    if (!f.open(QIODevice::ReadOnly)) {
        return; // chưa có file (lần đầu) -> giữ default
    }

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "OtaSettings: corrupt config file" << mPath << err.errorString();
        return;
    }
    mRoot = doc.object();
}

bool OtaSettings::save() const
{
    // Đảm bảo thư mục cha tồn tại (vd /data/ota).
    const QString dir = QFileInfo(mPath).absolutePath();
    if (!QDir().mkpath(dir)) {
        qWarning() << "OtaSettings: cannot create dir" << dir;
        return false;
    }

    QFile f(mPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "OtaSettings: cannot write" << mPath << f.errorString();
        return false;
    }
    f.write(QJsonDocument(mRoot).toJson(QJsonDocument::Indented));
    return true;
}

bool OtaSettings::autoMode() const
{
    return mRoot.value(QStringLiteral("autoMode")).toBool(false);
}

void OtaSettings::setAutoMode(bool autoMode)
{
    mRoot.insert(QStringLiteral("autoMode"), autoMode);
}
