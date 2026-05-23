#include "MainDashboard.h"
#include "./ui_MainDashboard.h"

#include <QPushButton>

#define LIGHT_SENSOR_DEVICE_PATH                "/dev/i2c-1"
#define LIGHT_SENSOR_DEVICE_ADDRESS             0x23

#define TEMPERATURE_HUMIDITY_DEVICE_PATH        "/dev/i2c-1"
#define TEMPERATURE_HUMIDITY_DEVICE_ADDRESS     0x44

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);

    QObject::connect(ui->settingsButton, &QPushButton::clicked,
                     this, &Widget::settingsRequested);
}

Widget::~Widget()
{
    delete ui;
}

int Widget::init()
{
    int ret;

    ret = mLightSensor.init(LIGHT_SENSOR_DEVICE_PATH, LIGHT_SENSOR_DEVICE_ADDRESS);
    if (ret < 0)
    {
        qWarning() << "Failed to init light sensor";
        return -1;
    }

    ret = mTemperatureHumiditySensor.init(TEMPERATURE_HUMIDITY_DEVICE_PATH, TEMPERATURE_HUMIDITY_DEVICE_ADDRESS);
    if (ret < 0)
    {
        qWarning() << "Failed to init temperature humidity sensor";
        return -1;
    }

    QObject::connect(&client, &MqttClient::connected, [this]() {
        qDebug() << "Connected to MQTT broker";
        emit onlineChanged(true);
    });

    QObject::connect(&client, &MqttClient::disconnected, [this](int rc) {
        qDebug() << "Disconnected from MQTT broker, rc =" << rc;
        emit onlineChanged(false);
    });

    QObject::connect(&client, &MqttClient::messageReceived, [](const QString &topic, const QByteArray &payload) {
        qDebug() << "Received message:" << topic << payload;
    });

    QObject::connect(&client, &MqttClient::errorOccurred, [this](const QString &err) {
        qWarning() << "MQTT error:" << err;
        emit onlineChanged(false);
    });

    QObject::connect(&mReadSensorDataTimer, &QTimer::timeout, [this]() {
        TemperatureHumiditySensor::CelsiusHumidityValue celsiusHumidityValue;
        int luxValue;

        luxValue = mLightSensor.readLuxValue();
        celsiusHumidityValue = mTemperatureHumiditySensor.readCelsiusHumidityValue();

        ui->tempValue->setText(QString::number((double)celsiusHumidityValue.temperature, 'f', 1));
        ui->humValue->setText(QString::number((int)celsiusHumidityValue.humidity));
        ui->illValue->setText(QString::number(luxValue));

        if (client.isConnected()) {
            client.publishMessage(
                "sensor/temp",
                QByteArray::number((double)celsiusHumidityValue.temperature, 'f', 1),
                0,
                true
            );
            client.publishMessage(
                "sensor/humi",
                QByteArray::number((int)celsiusHumidityValue.humidity),
                0,
                true
            );
            client.publishMessage(
                "sensor/lux",
                QByteArray::number(luxValue),
                0,
                true
            );
        }
    });

    mReadSensorDataTimer.start(3000);

    bool portOk = false;
    const QString brokerHost = qEnvironmentVariable("MQTT_BROKER_HOST", QStringLiteral("127.0.0.1"));
    int brokerPort = qEnvironmentVariableIntValue("MQTT_BROKER_PORT", &portOk);
    if (!portOk || brokerPort <= 0) brokerPort = 1883;
    client.connectToHost(brokerHost, brokerPort, 60);
    
    return 0;
}
