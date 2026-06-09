#include "MqttSettings.h"
#include "OtaConfig.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>

MqttSettings::MqttSettings(const QString &path)
    : mPath(path.isEmpty() ? OtaConfig::mqttConfigFile() : path)
{
}

void MqttSettings::load()
{
    QFile f(mPath);
    if (!f.open(QIODevice::ReadOnly)) {
        return;
    }

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "MqttSettings: corrupt config file" << mPath << err.errorString();
        return;
    }
    mRoot = doc.object();
}

bool MqttSettings::save() const
{
    const QString dir = QFileInfo(mPath).absolutePath();
    if (!QDir().mkpath(dir)) {
        qWarning() << "MqttSettings: cannot create dir" << dir;
        return false;
    }

    QFile f(mPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "MqttSettings: cannot write" << mPath << f.errorString();
        return false;
    }
    f.write(QJsonDocument(mRoot).toJson(QJsonDocument::Indented));
    return true;
}

QString MqttSettings::host() const
{
    return mRoot.value(QStringLiteral("host")).toString(QStringLiteral("192.168.137.10"));
}

void MqttSettings::setHost(const QString &host)
{
    mRoot.insert(QStringLiteral("host"), host);
}

int MqttSettings::port() const
{
    return mRoot.value(QStringLiteral("port")).toInt(1883);
}

void MqttSettings::setPort(int port)
{
    mRoot.insert(QStringLiteral("port"), port);
}
