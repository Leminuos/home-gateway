#ifndef SETTINGSDASHBOARD_H
#define SETTINGSDASHBOARD_H

#include <QWidget>

#include "drivers/BacklightPwm.h"

QT_BEGIN_NAMESPACE
namespace Ui { class SettingsWidget; }
QT_END_NAMESPACE

class SettingsWidget : public QWidget
{
    Q_OBJECT

public:
    SettingsWidget(QWidget *parent = nullptr);
    ~SettingsWidget();

public slots:
    // Đồng bộ trạng thái toggle Auto/Manual với cấu hình đã lưu (/data).
    void setOtaMode(bool autoMode);
    // Hiển thị version firmware hiện tại (đọc từ /etc/sw-versions qua OtaManager).
    void setCurrentVersion(const QString &version);
    // Phản hồi cho nút "Check for updates" (mode manual).
    void onCheckStarted();
    void onCheckFinished(const QString &resultText);

signals:
    void backRequested();
    void otaAutoModeChanged(bool autoMode);  // toggle Auto/Manual
    void checkForUpdateRequested();          // nút "Check for updates"

private:
    void onBrightnessChanged(int value);
    void onOtaToggled(bool checked);

    Ui::SettingsWidget *ui;
    BacklightPwm mBacklightPwm;
};

#endif // SETTINGSDASHBOARD_H
