#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>

#include "drivers/LightSensor.h"
#include "drivers/TemperatureHumiditySensor.h"
#include "logging/SensorLogger.h"
#include "network/MqttClient.h"

#include <QTimer>
#include <QFile>
#include <QDebug>

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();
    int init(const QString &mqttHost, int mqttPort);
    void deinit();

    // Nguồn lịch sử số liệu cảm biến cho ChartDashboard (load + signal).
    SensorLogger &sensorLogger() { return mSensorLogger; }

signals:
    void onlineChanged(bool online);

private:
    Ui::Widget *ui;

    QTimer mReadSensorDataTimer;

    MqttClient client;
    LightSensor mLightSensor;
    TemperatureHumiditySensor mTemperatureHumiditySensor;
    SensorLogger mSensorLogger;
};
#endif // WIDGET_H
