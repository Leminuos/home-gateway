#include "FirmwareUpdatePopup.h"
#include "./ui_FirmwareUpdatePopup.h"

#include <QPushButton>

FirmwareUpdatePopup::FirmwareUpdatePopup(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::FirmwareUpdatePopup)
{
    ui->setupUi(this);

    QObject::connect(ui->updateNowButton, &QPushButton::clicked, [this]() {
        emit updateConfirmed();
        hide();
    });
    
    QObject::connect(ui->cancelUpdateButton, &QPushButton::clicked, [this]() {
        emit cancelled();
        hide();
    });
}

FirmwareUpdatePopup::~FirmwareUpdatePopup()
{
    delete ui;
}

void FirmwareUpdatePopup::setVersions(const QString &current, const QString &available)
{
    ui->firmwareOldVersion->setText(current);
    ui->firmwareNewVersion->setText(available);
}
