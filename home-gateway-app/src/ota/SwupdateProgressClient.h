#ifndef SWUPDATEPROGRESSCLIENT_H
#define SWUPDATEPROGRESSCLIENT_H

#include <QByteArray>
#include <QLocalSocket>
#include <QObject>
#include <QString>

// Đọc tiến độ install từ socket SWUpdate (/tmp/swupdateprog) — interface mà
// swupdate-progress và web UI dùng. Connect ngay trước khi POST .swu để bắt
// toàn bộ verify + flash và biết khi nào xong/lỗi.
//
// ABI: layout progress_msg bám SWUpdate 2022.05 (kirkstone). Đổi version mà
// struct đổi -> sửa struct trong .cpp. Có sanity-check status để bỏ message rác.
class SwupdateProgressClient : public QObject
{
    Q_OBJECT

public:
    explicit SwupdateProgressClient(QObject *parent = nullptr);

    void start();  // connect socket; an toàn khi gọi lại
    void stop();

signals:
    void progress(int percent);              // % bước hiện tại (verify/flash)
    void succeeded();                        // flash slot inactive xong
    void failed(const QString &info);        // daemon báo lỗi
    void connectionError(const QString &reason); // không mở được socket

private:
    void onReadyRead();
    void onError(QLocalSocket::LocalSocketError err);

    QLocalSocket mSocket;
    QByteArray mBuffer;
    bool mFinished; // đã emit succeeded/failed cho lần update hiện tại chưa
};

#endif // SWUPDATEPROGRESSCLIENT_H
