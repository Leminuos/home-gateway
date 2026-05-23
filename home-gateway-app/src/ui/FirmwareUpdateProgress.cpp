#include "FirmwareUpdateProgress.h"
#include "./ui_FirmwareUpdateProgress.h"

#include <QLabel>
#include <QPushButton>
#include <QStyle>

// Màn hình tiến độ update (view thuần, OtaManager lái).
// 3 step: Downloading / Verifying / Flashing -> Complete (nút Reboot now).

namespace {

// Re-apply stylesheet để selector động [state]/[variant] có hiệu lực.
void repolish(QWidget *w)
{
    w->style()->unpolish(w);
    w->style()->polish(w);
    w->update();
}

constexpr const char *kStatePending = "pending";
constexpr const char *kStateActive  = "active";
constexpr const char *kStateDone    = "done";

// Thứ tự 3 step dùng để reset hàng loạt về "pending" khi start().
const FirmwareUpdateProgress::Phase kStepPhases[] = {
    FirmwareUpdateProgress::PhaseDownloading,
    FirmwareUpdateProgress::PhaseVerifying,
    FirmwareUpdateProgress::PhaseFlashing,
};

} // namespace


// -----------------------------------------------------------------------------
// Khởi tạo / huỷ
// -----------------------------------------------------------------------------

FirmwareUpdateProgress::FirmwareUpdateProgress(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::FirmwareUpdateProgress)
    , mPhase(PhaseIdle)
{
    ui->setupUi(this);

    QObject::connect(ui->actionButton, &QPushButton::clicked,
                     this, &FirmwareUpdateProgress::onActionClicked);
}

FirmwareUpdateProgress::~FirmwareUpdateProgress()
{
    delete ui;
}


// -----------------------------------------------------------------------------
// Public slots: từng bước của luồng update
// -----------------------------------------------------------------------------

void FirmwareUpdateProgress::start(const QString &targetVersion)
{
    mTargetVersion = targetVersion;
    mPhase = PhaseDownloading;

    for (Phase p : kStepPhases) {
        markStep(p, kStatePending, -1);
    }

    setPhaseHeader("Downloading...", "Fetching firmware from server");
    markStep(PhaseDownloading, kStateActive, 0);
    setActionButton("Cancel", "cancel", /*enabled=*/true);
}

void FirmwareUpdateProgress::setDownloadProgress(int percent)
{
    if (mPhase == PhaseDownloading) {
        markStep(PhaseDownloading, kStateActive, percent);
    }
}

void FirmwareUpdateProgress::enterVerify()
{
    mPhase = PhaseVerifying;
    markStep(PhaseDownloading, kStateDone, -1);
    setPhaseHeader("Verifying...", "Sending to updater");
    markStep(PhaseVerifying, kStateActive, -1);
    setActionButton("Cancel", "cancel", /*enabled=*/true);
}

void FirmwareUpdateProgress::enterFlash()
{
    mPhase = PhaseFlashing;
    markStep(PhaseVerifying, kStateDone, -1);
    setPhaseHeader("Flashing...", "Writing to flash · do not power off");
    markStep(PhaseFlashing, kStateActive, 0);
    // KHÔNG cho Cancel: cắt nguồn lúc này có thể làm hỏng slot đang ghi.
    setActionButton("Cancel", "cancel", /*enabled=*/false);
}

void FirmwareUpdateProgress::setFlashProgress(int percent)
{
    if (mPhase == PhaseFlashing) {
        markStep(PhaseFlashing, kStateActive, percent);
    }
}

void FirmwareUpdateProgress::complete()
{
    mPhase = PhaseComplete;
    markStep(PhaseFlashing, kStateDone, -1);
    setPhaseHeader("Update complete", mTargetVersion + " installed");
    setActionButton("Reboot now", "primary", /*enabled=*/true);
}

void FirmwareUpdateProgress::fail(const QString &reason)
{
    mPhase = PhaseFailed;
    // Giữ nguyên trạng thái step để user thấy hỏng ở bước nào.
    setPhaseHeader("Update failed", reason);
    setActionButton("Back", "cancel", /*enabled=*/true);
}


// -----------------------------------------------------------------------------
// Event handler
// -----------------------------------------------------------------------------

// onActionClicked(): nút action duy nhất của panel.
//   - Complete -> emit rebootRequested() để bên ngoài thực thi reboot thật.
//   - Failed / các phase đang chạy có Cancel -> về Idle và emit cancelled().
void FirmwareUpdateProgress::onActionClicked()
{
    if (mPhase == PhaseComplete) {
        // Phản hồi cho user: đã nhận lệnh, thiết bị đang khởi động lại.
        // (nút disable để không bấm lại; thiết bị sẽ reboot sau giây lát)
        setPhaseHeader("Rebooting...", "Device is restarting");
        setActionButton("Rebooting...", "primary", /*enabled=*/false);
        emit rebootRequested();
        return;
    }

    mPhase = PhaseIdle;
    emit cancelled();
}


// -----------------------------------------------------------------------------
// UI helpers
// -----------------------------------------------------------------------------

// markStep(): cập nhật trạng thái hiển thị của một step (icon + label + percent).
// `state` là một trong "pending" / "active" / "done".
// Khi state == "active" và progress >= 0, label phần trăm hiển thị "<progress>%".
void FirmwareUpdateProgress::markStep(Phase phase, const QString &state, int progress)
{
    QLabel *icon    = nullptr;
    QLabel *label   = nullptr;
    QLabel *percent = nullptr;

    switch (phase) {
    case PhaseDownloading:
        icon = ui->downloadIcon; label = ui->downloadLabel; percent = ui->downloadPercent;
        break;
    case PhaseVerifying:
        icon = ui->verifyIcon;   label = ui->verifyLabel;   percent = ui->verifyPercent;
        break;
    case PhaseFlashing:
        icon = ui->flashIcon;    label = ui->flashLabel;    percent = ui->flashPercent;
        break;
    default:
        return;
    }

    setStepState(icon, label, percent, state, progress);
}

// setStepState(): thao tác trực tiếp lên 3 widget của một step. Set property
// "state" cho stylesheet đổi màu, set icon (✓ khi done), set phần trăm.
void FirmwareUpdateProgress::setStepState(QLabel *icon, QLabel *label, QLabel *percent,
                                          const QString &state, int progress)
{
    icon->setProperty("state", state);
    label->setProperty("state", state);
    percent->setProperty("state", state);

    icon->setText(state == kStateDone ? QStringLiteral("✓") : QString());

    if (state == kStateActive && progress >= 0) {
        percent->setText(QString::number(progress) + "%");
    } else {
        percent->setText(QString());
    }

    repolish(icon);
    repolish(label);
    repolish(percent);
}

// setPhaseHeader(): đặt tiêu đề lớn + subtitle nhỏ phía trên danh sách step.
void FirmwareUpdateProgress::setPhaseHeader(const QString &title, const QString &subtitle)
{
    ui->phaseTitle->setText(title);
    ui->phaseSubtitle->setText(subtitle);
}

// setActionButton(): cấu hình nút action duy nhất của panel.
void FirmwareUpdateProgress::setActionButton(const QString &text, const QString &variant, bool enabled)
{
    ui->actionButton->setText(text);
    ui->actionButton->setProperty("variant", variant);
    ui->actionButton->setEnabled(enabled);
    repolish(ui->actionButton);
}
