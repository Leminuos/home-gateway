#include "AppSettings.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>

// Slider mặc định 72% (khớp giá trị khởi tạo trong SettingsDashboard.ui).
static const int kDefaultBrightness = 72;

AppSettings::AppSettings(const QString &path)
    : mPath(path.isEmpty()
            ? qEnvironmentVariable("APP_CONFIG_FILE",
                                   QStringLiteral("/data/config/setting.json"))
            : path)
{
}

void AppSettings::load()
{
    QFile f(mPath);
    if (!f.open(QIODevice::ReadOnly)) {
        return; // chưa có file (lần đầu) -> giữ default
    }

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "AppSettings: corrupt config file" << mPath << err.errorString();
        return;
    }
    mRoot = doc.object();
}

bool AppSettings::save() const
{
    const QString dir = QFileInfo(mPath).absolutePath();
    if (!QDir().mkpath(dir)) {
        qWarning() << "AppSettings: cannot create dir" << dir;
        return false;
    }

    QFile f(mPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "AppSettings: cannot write" << mPath << f.errorString();
        return false;
    }
    f.write(QJsonDocument(mRoot).toJson(QJsonDocument::Indented));
    return true;
}

int AppSettings::brightness() const
{
    return mRoot.value(QStringLiteral("brightness")).toInt(kDefaultBrightness);
}

void AppSettings::setBrightness(int percent)
{
    mRoot.insert(QStringLiteral("brightness"), percent);
}
