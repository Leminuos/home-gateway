#include "OtaManager.h"
#include "OtaConfig.h"

#include <QDebug>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegExp>
#include <QStringList>
#include <QTextStream>
#include <QUrl>

OtaManager::OtaManager(QObject *parent)
    : QObject(parent)
    , mNet(new QNetworkAccessManager(this))
    , mMqtt(QStringLiteral("bbb-ota-client"))
    , mAutoMode(false)
    , mManifestReply(nullptr)
    , mEnteredFlash(false)
    , mFlashPctMax(-1)
    , mInstallConcluded(false)
    , mManifestFromUser(true)
{
    connect(&mMqtt, &MqttClient::connected, this, &OtaManager::onMqttConnected);
    connect(&mMqtt, &MqttClient::messageReceived, this, &OtaManager::onMqttMessage);

    // Tiến độ download/flash đọc từ socket SWUpdate
    connect(&mSwu, &SwupdateProgressClient::downloadProgress, this, [this](int pct) {
        if (!mInstallConcluded) {
            emit downloadPercent(pct);
        }
    });
    connect(&mSwu, &SwupdateProgressClient::flashProgress,
            this, &OtaManager::onFlashProgress);
    connect(&mSwu, &SwupdateProgressClient::failed, this, [this](const QString &info) {
        concludeFailure(info.isEmpty() ? QStringLiteral("Install failed")
                                       : QStringLiteral("Install failed: ") + info);
    });
    connect(&mSwu, &SwupdateProgressClient::connectionError, this, [](const QString &r) {
        // Mất socket tiến độ -> vẫn kết luận được qua exit code, chỉ thiếu %.
        qWarning() << "SWUpdate progress socket:" << r << "- progress unavailable";
    });

    connect(&mInstallProc,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &OtaManager::onInstallFinished);
    connect(&mInstallProc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            concludeFailure(QStringLiteral("Cannot start updater"));
        }
    });

    connect(&mAutoPoller, &QTimer::timeout, this, [this]() {
        checkForUpdate(false);
    });
}

OtaManager::~OtaManager() = default;

// -----------------------------------------------------------------------------
// Khởi động
// -----------------------------------------------------------------------------

void OtaManager::start(const QString &brokerHost, int brokerPort)
{
    // Khôi phục cấu hình đã lưu ở /data (sống sót qua OTA + reboot).
    mSettings.load();
    mAutoMode = mSettings.autoMode();
    emit autoModeChanged(mAutoMode); // để toggle ở Settings phản ánh đúng

    mCurrentVersion = readCurrentVersion();
    emit currentVersionChanged(mCurrentVersion);

    mMqtt.connectToHost(brokerHost, brokerPort, 60);

    if (mAutoMode) {
        const int interval = mSettings.pollingIntervalSec();
        if (interval > 0) {
            mAutoPoller.setInterval(interval * 1000);
            mAutoPoller.start();
        }
    }
}

QString OtaManager::readCurrentVersion() const
{
    QFile f(OtaConfig::currentVersionFile());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Cannot read" << OtaConfig::currentVersionFile();
        return QStringLiteral("unknown");
    }

    QTextStream ts(&f);
    while (!ts.atEnd()) {
        const QString line = ts.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }
        // sw-versions: "<name> <version>" hoặc chỉ "<version>".
        const QStringList parts = line.split(QRegExp("\\s+"), QString::SkipEmptyParts);
        if (!parts.isEmpty()) {
            return parts.last();
        }
    }
    return QStringLiteral("unknown");
}

// -----------------------------------------------------------------------------
// Phát hiện version
// -----------------------------------------------------------------------------

void OtaManager::onMqttConnected()
{
    mMqtt.subscribeTopic(mSettings.mqttTopic(), 1);
}

void OtaManager::onMqttMessage(const QString &topic, const QByteArray &payload)
{
    if (topic != mSettings.mqttTopic()) {
        return;
    }
    evaluateManifest(FirmwareManifest::fromJson(payload), /*fromManualCheck=*/false);
}

void OtaManager::checkForUpdate(bool fromUser)
{
    if (mManifestReply) {
        return;
    }
    mManifestFromUser = fromUser;
    mManifestReply = mNet->get(QNetworkRequest(QUrl(mSettings.manifestUrl())));
    connect(mManifestReply, &QNetworkReply::finished, this, &OtaManager::onManifestFinished);
}

void OtaManager::onManifestFinished()
{
    QNetworkReply *reply = mManifestReply;
    mManifestReply = nullptr;
    if (!reply) {
        return;
    }
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        if (mManifestFromUser) {
            emit checkFailed(reply->errorString());
        }
        return;
    }
    evaluateManifest(FirmwareManifest::fromJson(reply->readAll()), mManifestFromUser);
}

void OtaManager::evaluateManifest(const FirmwareManifest &m, bool fromManualCheck)
{
    if (!m.isValid()) {
        if (fromManualCheck) {
            emit checkFailed(QStringLiteral("Manifest không hợp lệ"));
        }
        return;
    }

    mLatest = m;
    const bool newer = mSettings.forceUpdate()
                       || FirmwareManifest::compareVersion(m.version, mCurrentVersion) > 0;

    if (fromManualCheck) {
        if (newer) {
            emit updateAvailable(mCurrentVersion, m.version);
        } else {
            emit upToDate();
        }
        return;
    }

    // Đến từ MQTT push: chỉ tự bật popup khi đang ở chế độ auto.
    if (newer && mAutoMode) {
        emit updateAvailable(mCurrentVersion, m.version);
    }
}

void OtaManager::setAutoMode(bool autoMode)
{
    if (mAutoMode != autoMode) {
        mAutoMode = autoMode;
        mSettings.setAutoMode(autoMode);
        mSettings.save();

        if (mAutoMode) {
            const int interval = mSettings.pollingIntervalSec();
            if (interval > 0) {
                mAutoPoller.setInterval(interval * 1000);
                mAutoPoller.start();
            }
        } else {
            mAutoPoller.stop();
        }
    }
    // Vừa bật auto mà đã biết có bản mới (từ MQTT trước đó) -> popup luôn.
    if (mAutoMode && mLatest.isValid()
        && FirmwareManifest::compareVersion(mLatest.version, mCurrentVersion) > 0) {
        emit updateAvailable(mCurrentVersion, mLatest.version);
    }
}

// -----------------------------------------------------------------------------
// Thực thi update: giao URL cho SWUpdate downloader -> tải + flash
// -----------------------------------------------------------------------------

void OtaManager::confirmUpdate()
{
    if (mInstallProc.state() != QProcess::NotRunning) {
        return; // đang chạy
    }
    if (!mLatest.isValid()) {
        emit phaseFailed(QStringLiteral("No firmware info"));
        return;
    }

    resetInstallState();
    emit phaseDownload();

    mSwu.start();

    mInstallProc.start(OtaConfig::swupdateDownloadTool(), {mLatest.url});
}

void OtaManager::onFlashProgress(int percent)
{
    if (mInstallConcluded) {
        return;
    }

    if (!mEnteredFlash) {
        mEnteredFlash = true;
        emit phaseFlash();
    }

    if (percent > 99) percent = 99;   // 100% chỉ khi cài xong thật (exit 0)

    // SWUpdate báo % theo từng step (cur_percent reset 0 mỗi step mới: ghi
    // rootfs -> chạy switch-slot.sh...). Chỉ cho thanh tăng để không tụt về 0.
    if (percent <= mFlashPctMax) {
        return;
    }
    mFlashPctMax = percent;
    emit flashPercent(percent);
}

void OtaManager::onInstallFinished(int exitCode, QProcess::ExitStatus status)
{
    if (status == QProcess::NormalExit && exitCode == 0) {
        concludeSuccess();
        return;
    }

    // Ưu tiên thông báo lỗi chi tiết từ stderr của swupdate, nếu có.
    const QString err = QString::fromUtf8(mInstallProc.readAllStandardError()).trimmed();
    concludeFailure(err.isEmpty()
                        ? QStringLiteral("Update failed")
                        : QStringLiteral("Update failed: ") + err.section('\n', -1));
}

void OtaManager::concludeSuccess()
{
    if (mInstallConcluded) {
        return;
    }
    mInstallConcluded = true;
    mSwu.stop();

    if (!mEnteredFlash) {
        mEnteredFlash = true;
        emit phaseFlash();
    }
    emit flashPercent(100);
    emit phaseComplete();
}

void OtaManager::concludeFailure(const QString &reason)
{
    if (mInstallConcluded) {
        return;
    }
    mInstallConcluded = true;
    mSwu.stop();
    emit phaseFailed(reason);
}

void OtaManager::resetInstallState()
{
    mEnteredFlash = false;
    mFlashPctMax = -1;
    mInstallConcluded = false;
}

// -----------------------------------------------------------------------------
// Huỷ / reboot
// -----------------------------------------------------------------------------

void OtaManager::cancel()
{
    if (mInstallProc.state() != QProcess::NotRunning) {
        mInstallProc.kill();
        mInstallProc.waitForFinished(2000);
    }

    mSwu.stop();
    mInstallConcluded = true;
    mEnteredFlash = false;
}

void OtaManager::reboot()
{
    // App chạy bằng root (service không set User=) nên gọi reboot trực tiếp.
    QProcess::startDetached(QStringLiteral("reboot"), {});
}
