#include "SwupdateProgressClient.h"
#include "OtaConfig.h"

#include <QDebug>

#include <cstring>

namespace {

// Layout struct progress_msg của SWUpdate (network_ipc.h, 2022.05 / kirkstone).
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

} // namespace


SwupdateProgressClient::SwupdateProgressClient(QObject *parent)
    : QObject(parent)
    , mFinished(false)
{
    connect(&mSocket, &QLocalSocket::readyRead,
            this, &SwupdateProgressClient::onReadyRead);
    connect(&mSocket, &QLocalSocket::errorOccurred,
            this, &SwupdateProgressClient::onError);
}

void SwupdateProgressClient::start()
{
    stop();
    mBuffer.clear();
    mFinished = false;
    mSocket.connectToServer(OtaConfig::swupdateProgressSocket());
}

void SwupdateProgressClient::stop()
{
    if (mSocket.state() != QLocalSocket::UnconnectedState) {
        mSocket.abort();
    }
}

void SwupdateProgressClient::onError(QLocalSocket::LocalSocketError err)
{
    Q_UNUSED(err);
    emit connectionError(mSocket.errorString());
}

void SwupdateProgressClient::onReadyRead()
{
    mBuffer.append(mSocket.readAll());

    // Daemon stream nhiều struct liên tiếp; xử lý từng struct trọn vẹn.
    while (mBuffer.size() >= static_cast<int>(sizeof(progress_msg))) {
        progress_msg msg;
        std::memcpy(&msg, mBuffer.constData(), sizeof(progress_msg));
        mBuffer.remove(0, sizeof(progress_msg));

        // status ngoài dải hợp lệ -> message rác (lệch ABI), bỏ qua.
        if (msg.status > SWU_PROGRESS) {
            continue;
        }

        if (mFinished) {
            continue; // đã chốt kết quả
        }

        switch (msg.status) {
        case SWU_RUN:
        case SWU_PROGRESS:
        case SWU_DOWNLOAD: {
            int pct = static_cast<int>(msg.cur_percent);
            if (pct < 0) pct = 0;
            if (pct > 100) pct = 100;
            emit progress(pct);
            break;
        }
        case SWU_SUCCESS:
        case SWU_DONE:
            mFinished = true;
            emit progress(100);
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
            break; // IDLE / START / SUBPROCESS: không cần phản ứng
        }
    }
}
