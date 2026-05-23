#ifndef STATUSHEADER_H
#define STATUSHEADER_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui { class StatusHeader; }
QT_END_NAMESPACE

class StatusHeader : public QWidget
{
    Q_OBJECT

public:
    explicit StatusHeader(QWidget *parent = nullptr);
    ~StatusHeader();

    void setOnlineStatus(bool online);

private:
    Ui::StatusHeader *ui;
};

#endif // STATUSHEADER_H
