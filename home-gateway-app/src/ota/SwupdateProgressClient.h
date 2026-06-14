#ifndef SWUPDATEPROGRESSCLIENT_H
#define SWUPDATEPROGRESSCLIENT_H

#include <QByteArray>
#include <QLocalSocket>
#include <QObject>
#include <QString>
#include <QTimer>

// Đọc tiến độ update từ socket SWUpdate (/tmp/swupdateprog) — interface mà
// swupdate-progress và web UI dùng. App gọi start() ngay khi spawn tiến trình
// swupdate one-shot (downloader) để bắt cả download lẫn flash và biết khi nào
// xong/lỗi. Vì socket do tiến trình one-shot tạo có độ trễ, start() tự thử
// kết nối lại vài lần trước khi báo connectionError.
//
// ABI bám SWUpdate 2025.05 (PROGRESS_API v2): khi kết nối, server gửi struct
// progress_connect_ack (magic "ACK") rồi mới stream progress_msg -> phải đọc/bỏ
// 8 byte ack trước khi parse, nếu không cả stream bị lệch. Đổi version mà struct
// đổi -> sửa struct trong .cpp.
class SwupdateProgressClient : public QObject
{
    Q_OBJECT

public:
    explicit SwupdateProgressClient(QObject *parent = nullptr);

    void start();  // connect socket (tự retry); an toàn khi gọi lại
    void stop();

signals:
    void downloadProgress(int percent);      // % SWUpdate đang tải .swu từ URL
    void flashProgress(int percent);         // % ghi vào slot inactive
    void succeeded();                        // flash slot inactive xong
    void failed(const QString &info);        // daemon báo lỗi
    void connectionError(const QString &reason); // hết retry vẫn không mở được

private:
    void tryConnect();
    void onConnected();
    void onReadyRead();
    void onError(QLocalSocket::LocalSocketError err);

    QLocalSocket mSocket;
    QByteArray mBuffer;
    QTimer mRetryTimer;
    int mRetriesLeft;
    bool mAckConsumed; // đã đọc/bỏ progress_connect_ack đầu stream chưa
    bool mFinished;    // đã emit succeeded/failed cho lần update hiện tại chưa
};

#endif // SWUPDATEPROGRESSCLIENT_H
