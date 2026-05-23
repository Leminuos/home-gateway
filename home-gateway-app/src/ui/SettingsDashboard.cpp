#include "SettingsDashboard.h"
#include "./ui_SettingsDashboard.h"

#include "network/NetworkInfo.h"

#include <QCheckBox>
#include <QDebug>
#include <QPushButton>
#include <QScroller>
#include <QScrollerProperties>
#include <QSlider>
#include <QTimer>

#define BACKLIGHT_PWM_CHIP_PATH     "/sys/class/pwm/pwmchip0"
#define BACKLIGHT_PWM_CHANNEL       0
#define BACKLIGHT_PWM_PERIOD_NS     1000000  /* 1 kHz - đủ cao để mắt không thấy flicker */
#define BACKLIGHT_MIN_PERCENT       10       /* Floor duty cycle để slider=0 vẫn sáng mờ */

#define DEVICE_NETWORK_INTERFACE    "eth0"

// Map slider 0..100 -> duty cycle %, theo CIE
// Precompute: Y = ((L+16)/116)^3 nếu L>8, ngược lại Y = L/903.3.
static const int kBrightnessLut[101] = {
     10,  10,  10,  10,  10,  10,  11,  11,  11,  11,  // slider 0..9
     11,  11,  11,  11,  12,  12,  12,  12,  12,  12,  // slider 10..19
     13,  13,  13,  13,  14,  14,  14,  15,  15,  15,  // slider 20..29
     16,  16,  16,  17,  17,  18,  18,  19,  19,  20,  // slider 30..39
     20,  21,  21,  22,  22,  23,  24,  24,  25,  26,  // slider 40..49
     27,  27,  28,  29,  30,  31,  32,  32,  33,  34,  // slider 50..59
     35,  36,  37,  38,  40,  41,  42,  43,  44,  45,  // slider 60..69
     47,  48,  49,  51,  52,  53,  55,  56,  58,  59,  // slider 70..79
     61,  63,  64,  66,  68,  69,  71,  73,  75,  77,  // slider 80..89
     79,  81,  83,  85,  87,  89,  91,  93,  95,  98,  // slider 90..99
    100,                                               // slider 100
};

static int sliderToDutyPercent(int sliderValue)
{
    if (sliderValue < 0) sliderValue = 0;
    if (sliderValue > 100) sliderValue = 100;
    return kBrightnessLut[sliderValue];
}

SettingsWidget::SettingsWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::SettingsWidget)
{
    ui->setupUi(this);

    QScroller::grabGesture(ui->settingsScroll->viewport(), QScroller::LeftMouseButtonGesture);
    QScroller *scroller = QScroller::scroller(ui->settingsScroll->viewport());
    QScrollerProperties props = scroller->scrollerProperties();

    // Lock chặt theo trục dọc
    props.setScrollMetric(QScrollerProperties::AxisLockThreshold, 1.0);
    props.setScrollMetric(QScrollerProperties::OvershootDragResistanceFactor, 0.5);
    props.setScrollMetric(QScrollerProperties::OvershootDragDistanceFactor, 0);
    props.setScrollMetric(QScrollerProperties::OvershootScrollDistanceFactor, 0);

    scroller->setScrollerProperties(props);

    QObject::connect(ui->backButton, &QPushButton::clicked,
                     this, &SettingsWidget::backRequested);
    QObject::connect(ui->brightnessSlider, &QSlider::valueChanged,
                     this, &SettingsWidget::onBrightnessChanged);
    QObject::connect(ui->otaToggle, &QCheckBox::toggled,
                     this, &SettingsWidget::onOtaToggled);
    QObject::connect(ui->checkUpdatesButton, &QPushButton::clicked,
                     this, &SettingsWidget::checkForUpdateRequested);

    /* Init PWM cho backlight và set duty cycle ban đầu */
    if (mBacklightPwm.init(BACKLIGHT_PWM_CHIP_PATH,
                           BACKLIGHT_PWM_CHANNEL,
                           BACKLIGHT_PWM_PERIOD_NS) < 0) {
        qWarning() << "Failed to init backlight PWM";
    } else {
        mBacklightPwm.setBrightness(sliderToDutyPercent(ui->brightnessSlider->value()));
    }

    /* Đọc MAC address của interface và hiển thị */
    const QString mac = NetworkInfo::readMacAddress(DEVICE_NETWORK_INTERFACE);
    if (!mac.isEmpty()) {
        ui->deviceMacValue->setText(mac);
    } else {
        ui->deviceMacValue->setText("--");
    }
}

void SettingsWidget::setOtaMode(bool autoMode)
{
    // setChecked chỉ phát toggled() khi giá trị đổi -> onOtaToggled tự cập nhật
    // nhãn + phát otaAutoModeChanged. Tránh vòng lặp vì OtaManager::setAutoMode
    // không phát lại autoModeChanged.
    ui->otaToggle->setChecked(autoMode);
}

void SettingsWidget::setCurrentVersion(const QString &version)
{
    ui->firmwareVersionValue->setText(version);
}

void SettingsWidget::onCheckStarted()
{
    ui->checkUpdatesButton->setEnabled(false);
    ui->checkUpdatesButton->setText("Checking…");
}

void SettingsWidget::onCheckFinished(const QString &resultText)
{
    ui->checkUpdatesButton->setEnabled(true);
    ui->checkUpdatesButton->setText(resultText);
    // Khôi phục nhãn mặc định sau một lúc để user đọc kịp kết quả.
    QTimer::singleShot(2500, ui->checkUpdatesButton, [this]() {
        ui->checkUpdatesButton->setText("Check for updates");
    });
}

SettingsWidget::~SettingsWidget()
{
    mBacklightPwm.deinit();
    delete ui;
}

void SettingsWidget::onBrightnessChanged(int value)
{
    ui->brightnessValue->setText(QString("%1%").arg(value));
    mBacklightPwm.setBrightness(sliderToDutyPercent(value));
}

void SettingsWidget::onOtaToggled(bool checked)
{
    if (checked) {
        ui->otaModeLabel->setText("Automatic");
        ui->otaDescLabel->setText("Install updates when available");
    } else {
        ui->otaModeLabel->setText("Manual");
        ui->otaDescLabel->setText("Confirm before updating");
    }
    emit otaAutoModeChanged(checked);
}
