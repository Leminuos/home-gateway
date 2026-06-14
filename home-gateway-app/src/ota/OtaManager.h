#ifndef OTAMANAGER_H
#define OTAMANAGER_H

#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>

#include "FirmwareManifest.h"
#include "OtaSettings.h"
#include "SwupdateProgressClient.h"
#include "network/MqttClient.h"

QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
class QNetworkReply;
QT_END_NAMESPACE

// Điều phối luồng OTA pull-model. KHÔNG đụng UI — chỉ phát signal cho main.cpp.
class OtaManager : public QObject
{
    Q_OBJECT

public:
    explicit OtaManager(QObject *parent = nullptr);
    ~OtaManager();

    // Đọc version hiện tại, kết nối MQTT (cùng broker với sensor) và subscribe
    // topic OTA. Gọi 1 lần lúc khởi động.
    void start(const QString &brokerHost, int brokerPort);

    QString currentVersion() const { return mCurrentVersion; }
    QString availableVersion() const { return mLatest.version; }

public slots:
    void setAutoMode(bool autoMode);
    void checkForUpdate(bool fromUser = true);  // fromUser=false: auto poll, không emit upToDate/checkFailed
    void confirmUpdate();    // user bấm "Update now": giao URL cho SWUpdate tải + cài
    void cancel();           // huỷ tải/cài, về idle
    void reboot();           // user bấm "Reboot now"

signals:
    void currentVersionChanged(const QString &version);
    // Phát lúc khởi động để UI (toggle) phản ánh chế độ đã lưu ở /data.
    void autoModeChanged(bool autoMode);

    // Phát hiện version
    void updateAvailable(const QString &current, const QString &available);
    void upToDate();                          // manual check: không có bản mới
    void checkFailed(const QString &reason);  // manual check lỗi mạng

    // Tiến độ cài đặt (lái FirmwareUpdateProgress)
    void phaseDownload();
    void downloadPercent(int percent);
    void phaseFlash();
    void flashPercent(int percent);
    void phaseComplete();
    void phaseFailed(const QString &reason);

private:
    void onMqttConnected();
    void onMqttMessage(const QString &topic, const QByteArray &payload);
    void onManifestFinished();
    void onFlashProgress(int percent);
    void onInstallFinished(int exitCode, QProcess::ExitStatus status);
    void concludeSuccess();
    void concludeFailure(const QString &reason);
    void resetInstallState();

    void evaluateManifest(const FirmwareManifest &m, bool fromManualCheck);
    QString readCurrentVersion() const;

    QNetworkAccessManager *mNet;
    MqttClient mMqtt;
    SwupdateProgressClient mSwu;
    QProcess mInstallProc;   // swupdate downloader one-shot

    FirmwareManifest mLatest;
    OtaSettings mSettings;
    QString mCurrentVersion;
    bool mAutoMode;

    QNetworkReply *mManifestReply;

    bool mEnteredFlash;
    int mFlashPctMax;        // % flash cao nhất đã hiển thị (giữ thanh chỉ tăng)
    bool mInstallConcluded;
    bool mManifestFromUser;  // phân biệt request từ user (true) hay auto poll (false)
    QTimer mAutoPoller;      // HTTP poll định kỳ khi ở chế độ auto
};

#endif // OTAMANAGER_H
