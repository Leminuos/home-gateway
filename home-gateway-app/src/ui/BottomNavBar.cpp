#include "BottomNavBar.h"
#include "./ui_BottomNavBar.h"

#include <QButtonGroup>

BottomNavBar::BottomNavBar(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::BottomNavBar)
    , mGroup(new QButtonGroup(this))
{
    ui->setupUi(this);

    mGroup->setExclusive(true);
    mGroup->addButton(ui->homeButton, Home);
    mGroup->addButton(ui->dashboardButton, Dashboard);
    mGroup->addButton(ui->settingsButton, Settings);

    ui->homeButton->setChecked(true);

    QObject::connect(mGroup, &QButtonGroup::idClicked,
                     this, &BottomNavBar::navRequested);
}

BottomNavBar::~BottomNavBar()
{
    delete ui;
}

void BottomNavBar::setActive(int index)
{
    QAbstractButton *btn = mGroup->button(index);
    if (btn) {
        btn->setChecked(true);
    }
}
