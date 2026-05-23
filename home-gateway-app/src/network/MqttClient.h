#ifndef __MQTT_CLIENT_H__
#define __MQTT_CLIENT_H__

#include <QObject>
#include <QTimer>
#include <QByteArray>
#include <QString>

extern "C" {
#include "mosquitto.h"
}

class MqttClient : public QObject
{
    Q_OBJECT
public:
    explicit MqttClient(const QString &clientId = QStringLiteral("bbb-qt-client"),
                        QObject *parent = nullptr);
    ~MqttClient();

    void connectToHost(const QString &host, int port = 1883, int keepalive = 60);
    void disconnectFromHost();

    void publishMessage(const QString &topic,
                        const QByteArray &payload,
                        int qos = 0,
                        bool retain = false);

    void subscribeTopic(const QString &topic, int qos = 0);
    
    bool isConnected(void);

signals:
    void connected();
    void disconnected(int rc);
    void messageReceived(const QString &topic, const QByteArray &payload);
    void errorOccurred(const QString &errorString);

private slots:
    void processLoop(); // được gọi bởi QTimer

private:
    struct mosquitto *m_mosq;
    QTimer m_loopTimer;
    QString m_host;
    int m_port;
    int m_keepalive;
    bool m_isConnected;

    static void on_connect(struct mosquitto *mosq, void *userdata, int rc);
    static void on_disconnect(struct mosquitto *mosq, void *userdata, int rc);
    static void on_message(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *msg);
};

#endif /* __MQTT_CLIENT_H__ */
