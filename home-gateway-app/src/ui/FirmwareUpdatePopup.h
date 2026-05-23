#ifndef FIRMWAREUPDATEPOPUP_H
#define FIRMWAREUPDATEPOPUP_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui { class FirmwareUpdatePopup; }
QT_END_NAMESPACE

class FirmwareUpdatePopup : public QWidget
{
    Q_OBJECT

public:
    explicit FirmwareUpdatePopup(QWidget *parent = nullptr);
    ~FirmwareUpdatePopup();

    void setVersions(const QString &current, const QString &available);

signals:
    void updateConfirmed();
    void cancelled();

private:
    Ui::FirmwareUpdatePopup *ui;
};

#endif // FIRMWAREUPDATEPOPUP_H
