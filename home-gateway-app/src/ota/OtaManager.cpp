#include "OtaManager.h"
#include "OtaConfig.h"

#include <QDebug>
#include <QHttpMultiPart>
#include <QHttpPart>
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
    , mDownloadReply(nullptr)
    , mUploadReply(nullptr)
    , mEnteredFlash(false)
    , mInstallConcluded(false)
{
    connect(&mMqtt, &MqttClient::connected, this, &OtaManager::onMqttConnected);
    connect(&mMqtt, &MqttClient::messageReceived, this, &OtaManager::onMqttMessage);

    // mSwu chỉ dùng để bắt success/failure; tiến độ % do uploadProgress lái
    // (đáng tin hơn, không lệ thuộc ABI của progress_msg).
    connect(&mSwu, &SwupdateProgressClient::succeeded, this, &OtaManager::concludeSuccess);
    connect(&mSwu, &SwupdateProgressClient::failed, this, [this](const QString &info) {
        concludeFailure(info.isEmpty() ? QStringLiteral("Install failed")
                                       : QStringLiteral("Install failed: ") + info);
    });
    connect(&mSwu, &SwupdateProgressClient::connectionError, this, [](const QString &r) {
        // Mất socket tiến độ -> dựa vào fallback poll ustate.
        qWarning() << "SWUpdate progress socket:" << r << "- falling back to ustate poll";
    });

    mUstatePoller.setInterval(2000);
    connect(&mUstatePoller, &QTimer::timeout, this, &OtaManager::pollUstate);
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
    // Topic retained -> message version mới nhất được gửi ngay khi subscribe.
    mMqtt.subscribeTopic(OtaConfig::mqttTopic(), 1);
}

void OtaManager::onMqttMessage(const QString &topic, const QByteArray &payload)
{
    if (topic != OtaConfig::mqttTopic()) {
        return;
    }
    evaluateManifest(FirmwareManifest::fromJson(payload), /*fromManualCheck=*/false);
}

void OtaManager::checkForUpdate()
{
    if (mManifestReply) {
        return; // đang check
    }
    mManifestReply = mNet->get(QNetworkRequest(QUrl(OtaConfig::manifestUrl())));
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
        emit checkFailed(reply->errorString());
        return;
    }
    evaluateManifest(FirmwareManifest::fromJson(reply->readAll()), /*fromManualCheck=*/true);
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
    // OTA_FORCE_UPDATE: ép luôn coi là có bản mới (test không cần bump version).
    const bool newer = OtaConfig::forceUpdate()
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
        // Lưu bền vững vào /data để giữ qua reboot/OTA.
        mSettings.setAutoMode(autoMode);
        mSettings.save();
    }
    // Vừa bật auto mà đã biết có bản mới (từ MQTT trước đó) -> popup luôn.
    if (mAutoMode && mLatest.isValid()
        && FirmwareManifest::compareVersion(mLatest.version, mCurrentVersion) > 0) {
        emit updateAvailable(mCurrentVersion, mLatest.version);
    }
}

// -----------------------------------------------------------------------------
// Thực thi update: tải -> POST SWUpdate -> flash
// -----------------------------------------------------------------------------

void OtaManager::confirmUpdate()
{
    if (mDownloadReply || mUploadReply) {
        return; // đang chạy
    }
    if (!mLatest.isValid()) {
        emit phaseFailed(QStringLiteral("No firmware info"));
        return;
    }

    resetInstallState();

    mDownloadFile.setFileName(OtaConfig::downloadPath());
    if (!mDownloadFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit phaseFailed(QStringLiteral("Cannot create temp file: ") + mDownloadFile.errorString());
        return;
    }

    emit phaseDownload();

    mDownloadReply = mNet->get(QNetworkRequest(QUrl(mLatest.url)));

    connect(mDownloadReply, &QNetworkReply::readyRead, this, [this]() {
        if (mDownloadFile.isOpen()) {
            mDownloadFile.write(mDownloadReply->readAll());
        }
    });
    connect(mDownloadReply, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) {
        if (total > 0) {
            emit downloadPercent(static_cast<int>(received * 100 / total));
        }
    });
    connect(mDownloadReply, &QNetworkReply::finished, this, &OtaManager::onDownloadFinished);
}

void OtaManager::onDownloadFinished()
{
    QNetworkReply *reply = mDownloadReply;
    mDownloadReply = nullptr;
    if (!reply) {
        return;
    }
    reply->deleteLater();

    if (mDownloadFile.isOpen()) {
        mDownloadFile.write(reply->readAll());
        mDownloadFile.close();
    }

    if (reply->error() != QNetworkReply::NoError) {
        emit phaseFailed(QStringLiteral("Download failed: ") + reply->errorString());
        return;
    }

    emit downloadPercent(100); // chốt 100% trước khi rời phase Downloading

    // NOTE: verify SHA-256 đang TẮT tạm thời (theo yêu cầu). Bật lại: băm dần
    // file trong readyRead rồi so với mLatest.sha256 ở đây — xem git history.
    startInstall();
}

void OtaManager::startInstall()
{
    emit phaseVerify();

    // Mở kênh tiến độ TRƯỚC khi đẩy file để không bỏ lỡ message đầu.
    mSwu.start();

    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentTypeHeader,
                       QVariant(QStringLiteral("application/octet-stream")));
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant(QStringLiteral("form-data; name=\"file\"; filename=\"update.swu\"")));

    QFile *file = new QFile(OtaConfig::downloadPath());
    if (!file->open(QIODevice::ReadOnly)) {
        delete file;
        delete multiPart;
        concludeFailure(QStringLiteral("Cannot open .swu to upload"));
        return;
    }
    filePart.setBodyDevice(file);
    file->setParent(multiPart);
    multiPart->append(filePart);

    mUploadReply = mNet->post(QNetworkRequest(QUrl(OtaConfig::swupdateUploadUrl())), multiPart);
    multiPart->setParent(mUploadReply);

    // SWUpdate stream/ghi file theo tốc độ nhận, nên uploadProgress phản ánh
    // tiến độ flash thật -> lái phase Flashing.
    connect(mUploadReply, &QNetworkReply::uploadProgress, this, &OtaManager::onUploadProgress);
    connect(mUploadReply, &QNetworkReply::finished, this, &OtaManager::onUploadFinished);
}

void OtaManager::onUploadFinished()
{
    QNetworkReply *reply = mUploadReply;
    mUploadReply = nullptr;
    if (!reply) {
        return;
    }
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        concludeFailure(QStringLiteral("Upload to SWUpdate failed: ") + reply->errorString());
        return;
    }

    // Daemon đã nhận file và bắt đầu flash. Bật fallback poll ustate phòng khi
    // không đọc được socket tiến độ.
    mUstatePoller.start();
}

void OtaManager::onUploadProgress(qint64 sent, qint64 total)
{
    if (mInstallConcluded || total <= 0) {
        return;
    }
    // Byte đầu tiên chảy đi -> SWUpdate đã verify xong header, bắt đầu ghi.
    if (!mEnteredFlash) {
        mEnteredFlash = true;
        emit phaseFlash();
    }
    int pct = static_cast<int>(sent * 100 / total);
    if (pct > 99) pct = 99;   // 100% chỉ khi cài xong thật (ustate/SUCCESS)
    emit flashPercent(pct);
}

void OtaManager::pollUstate()
{
    // Sau khi switch-slot.sh chạy (postinst), ustate=1 => đã sẵn sàng boot slot
    // mới => coi như cài thành công. Tín hiệu này độc lập với ABI socket.
    QProcess p;
    p.start(QStringLiteral("fw_printenv"), {QStringLiteral("-n"), QStringLiteral("ustate")});
    if (!p.waitForFinished(1500)) {
        return;
    }
    const QString out = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
    if (out == QStringLiteral("1")) {
        concludeSuccess();
    }
}

void OtaManager::concludeSuccess()
{
    if (mInstallConcluded) {
        return;
    }
    mInstallConcluded = true;
    mUstatePoller.stop();
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
    mUstatePoller.stop();
    mSwu.stop();
    emit phaseFailed(reason);
}

void OtaManager::resetInstallState()
{
    mEnteredFlash = false;
    mInstallConcluded = false;
}

// -----------------------------------------------------------------------------
// Huỷ / reboot
// -----------------------------------------------------------------------------

void OtaManager::cancel()
{
    if (mDownloadReply) {
        mDownloadReply->abort();
    }
    if (mUploadReply) {
        mUploadReply->abort();
    }
    if (mDownloadFile.isOpen()) {
        mDownloadFile.close();
    }
    mUstatePoller.stop();
    mSwu.stop();
    resetInstallState();
}

void OtaManager::reboot()
{
    // App chạy bằng root (service không set User=) nên gọi reboot trực tiếp.
    QProcess::startDetached(QStringLiteral("reboot"), {});
}
