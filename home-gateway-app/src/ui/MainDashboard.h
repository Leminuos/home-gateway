#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>

#include "drivers/LightSensor.h"
#include "drivers/TemperatureHumiditySensor.h"
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
    int init();
    void deinit();

signals:
    void settingsRequested();
    void onlineChanged(bool online);

private:
    Ui::Widget *ui;

    QTimer mReadSensorDataTimer;

    MqttClient client;
    LightSensor mLightSensor;
    TemperatureHumiditySensor mTemperatureHumiditySensor;
};
#endif // WIDGET_H
