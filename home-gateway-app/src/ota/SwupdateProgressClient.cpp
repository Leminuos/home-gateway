#include "SwupdateProgressClient.h"
#include "OtaConfig.h"

#include <QDebug>

#include <cstring>

namespace {

// Layout struct progress_msg của SWUpdate (progress_ipc.h, 2025.05).
// Cùng ABI compiler/target nên sizeof & offset khớp daemon.
enum SwuStatus {
    SWU_IDLE     = 0,
    SWU_START    = 1,
    SWU_RUN      = 2,
    SWU_SUCCESS  = 3,
    SWU_FAILURE  = 4,
    SWU_DOWNLOAD = 5,
    SWU_DONE     = 6,
    SWU_SUBPROCESS = 7,
    SWU_PROGRESS = 8,
};

constexpr int kImageNameLen = 256;
constexpr int kHandlerNameLen = 64;
constexpr int kInfoLen = 2048;

struct progress_msg {
    unsigned int       apiversion;            // API version của daemon
    unsigned int       status;                // RECOVERY_STATUS
    unsigned int       dwl_percent;           // % data đã download
    unsigned long long dwl_bytes;             // tổng byte đã download
    unsigned int       nsteps;                // tổng số step
    unsigned int       cur_step;              // step hiện tại
    unsigned int       cur_percent;           // % trong step hiện tại
    char               cur_image[kImageNameLen];
    char               hnd_name[kHandlerNameLen];
    unsigned int       source;                // nguồn trigger update
    unsigned int       infolen;               // độ dài hợp lệ trong info
    char               info[kInfoLen];
};

// Gửi 1 lần khi client vừa kết nối (PROGRESS_API v2), trước stream progress_msg.
struct progress_connect_ack {
    unsigned int apiversion;
    char         magic[4];   // "ACK"
};

} // namespace

constexpr int kRetryIntervalMs = 250;
constexpr int kMaxRetries = 40;

SwupdateProgressClient::SwupdateProgressClient(QObject *parent)
    : QObject(parent)
    , mRetriesLeft(0)
    , mAckConsumed(false)
    , mFinished(false)
{
    connect(&mSocket, &QLocalSocket::connected,
            this, &SwupdateProgressClient::onConnected);
    connect(&mSocket, &QLocalSocket::readyRead,
            this, &SwupdateProgressClient::onReadyRead);
    connect(&mSocket, &QLocalSocket::errorOccurred,
            this, &SwupdateProgressClient::onError);

    mRetryTimer.setSingleShot(true);
    mRetryTimer.setInterval(kRetryIntervalMs);
    connect(&mRetryTimer, &QTimer::timeout, this, &SwupdateProgressClient::tryConnect);
}

void SwupdateProgressClient::start()
{
    stop();
    mBuffer.clear();
    mFinished = false;
    mAckConsumed = false;
    mRetriesLeft = kMaxRetries;
    tryConnect();
}

void SwupdateProgressClient::stop()
{
    mRetryTimer.stop();
    mRetriesLeft = 0;

    if (mSocket.state() != QLocalSocket::UnconnectedState) {
        mSocket.abort();
    }
}

void SwupdateProgressClient::tryConnect()
{
    mSocket.abort();
    mSocket.connectToServer(OtaConfig::swupdateProgressSocket());
}

void SwupdateProgressClient::onConnected()
{
    mRetryTimer.stop();
}

void SwupdateProgressClient::onError(QLocalSocket::LocalSocketError err)
{
    Q_UNUSED(err);

    // Socket chưa kịp tạo -> thử lại; hết lượt mới báo lỗi kết nối.
    if (mRetriesLeft > 0) {
        --mRetriesLeft;
        mRetryTimer.start();
        return;
    }

    emit connectionError(mSocket.errorString());
}

void SwupdateProgressClient::onReadyRead()
{
    mBuffer.append(mSocket.readAll());

    // Đầu stream là progress_connect_ack (magic "ACK") — đọc/bỏ trước khi parse
    // progress_msg, nếu không toàn bộ struct sau sẽ lệch.
    if (!mAckConsumed) {
        if (mBuffer.size() < static_cast<int>(sizeof(progress_connect_ack))) {
            return; // chờ đủ 8 byte ack
        }
        progress_connect_ack ack;
        std::memcpy(&ack, mBuffer.constData(), sizeof(ack));
        if (std::strncmp(ack.magic, "ACK", sizeof(ack.magic)) != 0) {
            qWarning() << "SWUpdate progress: ack magic không khớp"
                       << QByteArray(ack.magic, sizeof(ack.magic));
        }
        mBuffer.remove(0, sizeof(progress_connect_ack));
        mAckConsumed = true;
    }

    // swupdate stream nhiều struct liên tiếp; xử lý từng struct trọn vẹn.
    while (mBuffer.size() >= static_cast<int>(sizeof(progress_msg))) {
        progress_msg msg;
        std::memcpy(&msg, mBuffer.constData(), sizeof(progress_msg));
        mBuffer.remove(0, sizeof(progress_msg));

        // status ngoài dải hợp lệ -> message rác (lệch ABI), bỏ qua.
        if (msg.status > SWU_PROGRESS) continue;

        if (mFinished) continue;

        auto clamp = [](unsigned int v) {
            int pct = static_cast<int>(v);
            if (pct < 0) pct = 0;
            if (pct > 100) pct = 100;
            return pct;
        };

        switch (msg.status) {
        case SWU_DOWNLOAD:
            // Giai đoạn curl tải .swu từ URL: % nằm ở dwl_percent.
            emit downloadProgress(clamp(msg.dwl_percent));
            break;
        
        case SWU_RUN:
        case SWU_PROGRESS:
            // Giai đoạn ghi vào slot inactive: % ở cur_percent.
            emit flashProgress(clamp(msg.cur_percent));
            break;
        
        case SWU_SUCCESS:
        case SWU_DONE:
            mFinished = true;
            emit flashProgress(100);
            emit succeeded();
            break;
        
        case SWU_FAILURE: {
            mFinished = true;
            QString info;
            if (msg.infolen > 0 && msg.infolen <= static_cast<unsigned>(kInfoLen)) {
                info = QString::fromUtf8(msg.info, static_cast<int>(msg.infolen));
            }
            emit failed(info);
            break;
        }

        default:
            break;
        }
    }
}
