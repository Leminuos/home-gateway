#ifndef BOTTOMNAVBAR_H
#define BOTTOMNAVBAR_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui { class BottomNavBar; }
QT_END_NAMESPACE

class QButtonGroup;

// Thanh điều hướng dưới cùng: Home / Dashboard / Settings.
// Phát navRequested(index) khi user chạm; setActive(index) để đồng bộ tab
// đang chọn khi điều hướng đến từ nơi khác (vd luồng OTA).
class BottomNavBar : public QWidget
{
    Q_OBJECT

public:
    enum Tab { Home = 0, Dashboard = 1, Settings = 2 };

    explicit BottomNavBar(QWidget *parent = nullptr);
    ~BottomNavBar();

public slots:
    void setActive(int index);

signals:
    void navRequested(int index);

private:
    Ui::BottomNavBar *ui;
    QButtonGroup *mGroup;
};

#endif // BOTTOMNAVBAR_H
