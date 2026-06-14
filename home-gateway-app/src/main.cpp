#include "ota/OtaManager.h"
#include "ota/MqttSettings.h"
#include "ui/BottomNavBar.h"
#include "ui/ChartDashboard.h"
#include "ui/FirmwareUpdatePopup.h"
#include "ui/FirmwareUpdateProgress.h"
#include "ui/MainDashboard.h"
#include "ui/SettingsDashboard.h"
#include "ui/StatusHeader.h"

#include <QApplication>
#include <QDebug>
#include <QFile>
#include <QStackedWidget>
#include <QTextStream>
#include <QVBoxLayout>
#include <QWidget>

static QString loadStyleSheet(const QString &resourcePath)
{
    QFile file(resourcePath);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        return QString();
    }
    const QString css = QTextStream(&file).readAll();
    file.close();
    return css;
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    a.setStyleSheet(loadStyleSheet(":/styles/dashboard.qss")
                    + "\n"
                    + loadStyleSheet(":/styles/settings.qss")
                    + "\n"
                    + loadStyleSheet(":/styles/firmware_popup.qss")
                    + "\n"
                    + loadStyleSheet(":/styles/firmware_progress.qss")
                    + "\n"
                    + loadStyleSheet(":/styles/chart.qss")
                    + "\n"
                    + loadStyleSheet(":/styles/bottomnav.qss"));

    QWidget root;
    root.setFixedSize(240, 320);
    root.setWindowTitle("Smart Home Dashboard");
    root.setObjectName("AppRoot");

    QVBoxLayout *rootLayout = new QVBoxLayout(&root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    StatusHeader *statusHeader = new StatusHeader(&root);
    rootLayout->addWidget(statusHeader);

    QStackedWidget *stack = new QStackedWidget(&root);
    rootLayout->addWidget(stack, 1);

    BottomNavBar *navBar = new BottomNavBar(&root);
    rootLayout->addWidget(navBar);

    Widget *dashboard = new Widget(stack);

    // Nạp lịch sử số liệu (CSV trên /data) trước khi dựng ChartDashboard để chart
    // vẽ ngay lịch sử có sẵn khi mở app.
    dashboard->sensorLogger().load();

    ChartDashboard *charts = new ChartDashboard(&dashboard->sensorLogger(), stack);
    SettingsWidget *settings = new SettingsWidget(stack);
    FirmwareUpdateProgress *progress = new FirmwareUpdateProgress(stack);

    // Thứ tự index khớp với BottomNavBar (Home=0, Dashboard=1, Settings=2).
    const int dashboardIndex = stack->addWidget(dashboard);
    const int chartsIndex = stack->addWidget(charts);
    const int settingsIndex = stack->addWidget(settings);
    const int progressIndex = stack->addWidget(progress);

    // Popup "có bản mới" là overlay phủ toàn màn hình, hiện được trên mọi screen
    // (cần cho chế độ auto). Là con trực tiếp của root, không nằm trong layout.
    FirmwareUpdatePopup *popup = new FirmwareUpdatePopup(&root);
    popup->hide();
    auto showPopup = [&]() {
        popup->setGeometry(root.rect());
        popup->raise();
        popup->show();
    };

    // Bộ điều phối OTA (pull-model). Không đụng UI; chỉ phát signal.
    OtaManager *ota = new OtaManager(&root);

    // ---- Điều hướng cơ bản giữa các screen --------------------------------
    QObject::connect(dashboard, &Widget::onlineChanged,
                     statusHeader, &StatusHeader::setOnlineStatus);

    // Bottom nav: index nút khớp index stack cho 3 screen chính.
    QObject::connect(navBar, &BottomNavBar::navRequested, [&](int index) {
        stack->setCurrentIndex(index);
    });

    // Cấp dữ liệu cho chart theo nhịp 1/phút (logger phát khi thực sự ghi mẫu).
    QObject::connect(&dashboard->sensorLogger(), &SensorLogger::sampleAdded,
                     charts, &ChartDashboard::appendSample);

    // ---- Settings <-> OtaManager ------------------------------------------
    QObject::connect(ota, &OtaManager::currentVersionChanged,
                     settings, &SettingsWidget::setCurrentVersion);
    // Chế độ auto/manual lưu ở /data: lúc start OtaManager phát autoModeChanged
    // để toggle phản ánh đúng; user gạt toggle thì lưu lại qua setAutoMode.
    QObject::connect(ota, &OtaManager::autoModeChanged,
                     settings, &SettingsWidget::setOtaMode);
    QObject::connect(settings, &SettingsWidget::otaAutoModeChanged,
                     ota, &OtaManager::setAutoMode);
    QObject::connect(settings, &SettingsWidget::checkForUpdateRequested, [&]() {
        settings->onCheckStarted();
        ota->checkForUpdate();
    });
    QObject::connect(ota, &OtaManager::upToDate, [&]() {
        settings->onCheckFinished("Up to date");
    });
    QObject::connect(ota, &OtaManager::checkFailed, [&](const QString &reason) {
        qWarning() << "OTA check failed:" << reason;
        settings->onCheckFinished("Check failed");
    });

    // ---- Phát hiện bản mới -> popup (auto: tự bật; manual: sau khi check) ---
    QObject::connect(ota, &OtaManager::updateAvailable,
                     [&](const QString &current, const QString &available) {
        settings->onCheckFinished("Check for updates"); // reset nhãn nút nếu đang manual
        popup->setVersions(current, available);
        showPopup();
    });

    // ---- Popup -> bắt đầu update ------------------------------------------
    // Trong lúc cập nhật ẩn nav bar để không cho điều hướng đi nơi khác.
    QObject::connect(popup, &FirmwareUpdatePopup::updateConfirmed, [&]() {
        popup->hide();
        progress->start(ota->availableVersion());
        stack->setCurrentIndex(progressIndex);
        navBar->hide();
        ota->confirmUpdate();
    });
    QObject::connect(popup, &FirmwareUpdatePopup::cancelled, [&]() {
        popup->hide();
    });

    // ---- OtaManager tiến độ -> FirmwareUpdateProgress ----------------------
    QObject::connect(ota, &OtaManager::downloadPercent,
                     progress, &FirmwareUpdateProgress::setDownloadProgress);
    QObject::connect(ota, &OtaManager::phaseFlash,
                     progress, &FirmwareUpdateProgress::enterFlash);
    QObject::connect(ota, &OtaManager::flashPercent,
                     progress, &FirmwareUpdateProgress::setFlashProgress);
    QObject::connect(ota, &OtaManager::phaseComplete,
                     progress, &FirmwareUpdateProgress::complete);
    QObject::connect(ota, &OtaManager::phaseFailed,
                     progress, &FirmwareUpdateProgress::fail);

    // ---- FirmwareUpdateProgress -> hành động ------------------------------
    QObject::connect(progress, &FirmwareUpdateProgress::cancelled, [&]() {
        ota->cancel();
        stack->setCurrentIndex(settingsIndex);
        navBar->setActive(BottomNavBar::Settings);
        navBar->show();
    });
    QObject::connect(progress, &FirmwareUpdateProgress::rebootRequested, [&]() {
        ota->reboot();
    });

    stack->setCurrentIndex(dashboardIndex);
    navBar->setActive(BottomNavBar::Home);
    root.show();

    MqttSettings mqttCfg;
    mqttCfg.load();
    const QString mqttHost = mqttCfg.host();
    const int mqttPort = mqttCfg.port();

    dashboard->init(mqttHost, mqttPort);
    ota->start(mqttHost, mqttPort);

    return a.exec();
}
