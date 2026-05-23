#ifndef FIRMWAREUPDATEPROGRESS_H
#define FIRMWAREUPDATEPROGRESS_H

#include <QString>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui { class FirmwareUpdateProgress; }
QT_END_NAMESPACE

class QLabel;

// Màn hình tiến độ update (view thuần) — OtaManager lái qua các public slot:
//   start -> Downloading -> Verifying -> Flashing -> complete() -> Complete
//   (nút Reboot now). fail() gọi được ở mọi phase.
// Không còn phase Rebooting: flash xong hiện luôn nút Reboot now (SWUpdate
// reboot-required = false, user chủ động reboot).
class FirmwareUpdateProgress : public QWidget
{
    Q_OBJECT

public:
    enum Phase {
        PhaseIdle,
        PhaseDownloading,
        PhaseVerifying,
        PhaseFlashing,
        PhaseComplete,
        PhaseFailed
    };

    explicit FirmwareUpdateProgress(QWidget *parent = nullptr);
    ~FirmwareUpdateProgress();

public slots:
    void start(const QString &targetVersion);  // -> Downloading
    void setDownloadProgress(int percent);
    void enterVerify();                         // Downloading done -> Verifying
    void enterFlash();                          // Verifying done -> Flashing
    void setFlashProgress(int percent);
    void complete();                            // Flashing done -> Complete
    void fail(const QString &reason);

signals:
    void cancelled();        // user huỷ (download/verify) hoặc đóng màn lỗi
    void rebootRequested();  // user bấm "Reboot now" ở màn Complete

private:
    void onActionClicked();

    void markStep(Phase phase, const QString &state, int progress);
    void setStepState(QLabel *icon, QLabel *label, QLabel *percent,
                      const QString &state, int progress);
    void setPhaseHeader(const QString &title, const QString &subtitle);
    void setActionButton(const QString &text, const QString &variant, bool enabled);

    Ui::FirmwareUpdateProgress *ui;
    Phase mPhase;
    QString mTargetVersion;
};

#endif // FIRMWAREUPDATEPROGRESS_H
