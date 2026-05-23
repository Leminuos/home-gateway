#include "StatusHeader.h"
#include "./ui_StatusHeader.h"

#include <QStyle>

StatusHeader::StatusHeader(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::StatusHeader)
{
    ui->setupUi(this);

    setOnlineStatus(false);
}

StatusHeader::~StatusHeader()
{
    delete ui;
}

void StatusHeader::setOnlineStatus(bool online)
{
    const QString state = online ? QStringLiteral("online") : QStringLiteral("offline");

    ui->dotLabel->setProperty("state", state);
    ui->onlineLabel->setProperty("state", state);
    ui->onlineLabel->setText(online ? "ONLINE" : "OFFLINE");

    for (QWidget *w : { static_cast<QWidget *>(ui->dotLabel),
                        static_cast<QWidget *>(ui->onlineLabel) }) {
        w->style()->unpolish(w);
        w->style()->polish(w);
        w->update();
    }
}
