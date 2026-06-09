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
    return mRoot.value(QStringLiteral("auto-mode")).toBool(false);
}

void OtaSettings::setAutoMode(bool autoMode)
{
    mRoot.insert(QStringLiteral("auto-mode"), autoMode);
}

QString OtaSettings::manifestUrl() const
{
    return mRoot.value(QStringLiteral("manifest-url"))
               .toString(QStringLiteral("http://192.168.137.10:8000/manifest.json"));
}

void OtaSettings::setManifestUrl(const QString &url)
{
    mRoot.insert(QStringLiteral("manifest-url"), url);
}

QString OtaSettings::mqttTopic() const
{
    return mRoot.value(QStringLiteral("mqtt-topic")).toString(QStringLiteral("ota/latest"));
}

void OtaSettings::setMqttTopic(const QString &topic)
{
    mRoot.insert(QStringLiteral("mqtt-topic"), topic);
}

bool OtaSettings::forceUpdate() const
{
    return mRoot.value(QStringLiteral("force-update")).toBool(false);
}

void OtaSettings::setForceUpdate(bool force)
{
    mRoot.insert(QStringLiteral("force-update"), force);
}

int OtaSettings::pollingIntervalSec() const
{
    return mRoot.value(QStringLiteral("polling-interval-sec")).toInt(60);
}

void OtaSettings::setPollingIntervalSec(int sec)
{
    mRoot.insert(QStringLiteral("polling-interval-sec"), sec);
}
