#include "mainwindow3.h"
#include "ui_mainwindow3.h"

#include <QDebug>
#include <QFontDatabase>

MainWindow3::MainWindow3(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MainWindow3)
{
    ui->setupUi(this);
    qDebug() << "Look up fonts";
    int idRegular = QFontDatabase::addApplicationFont(":/fonts/OpenSans-Regular.ttf");
    int idSemiBold = QFontDatabase::addApplicationFont(":/fonts/OpenSans-SemiBold.ttf");

    qDebug() << "Create fonts";
    QFont regular(QFontDatabase::applicationFontFamilies(idRegular).at(0));
    QFont semiBold(QFontDatabase::applicationFontFamilies(idSemiBold).at(0));
#ifdef Q_OS_MAC
    semiBold.setWeight(75);
#endif
    qDebug() << "Set fonts";
    ui->signature->init( "ALLKIRI", PageIcon::Style { semiBold, "/images/sign_dark_38x38.png", "#ffffff", "#998B66" },
        PageIcon::Style { regular, "/images/sign_light_38x38.png", "#023664", "#ffffff" }, true );
    ui->crypto->init( "KRÜPTO", PageIcon::Style { semiBold, "/images/crypto_dark_38x38.png", "#ffffff", "#998B66" },
        PageIcon::Style { regular, "/images/crypto_light_38x38.png", "#023664", "#ffffff" }, false );
    ui->myEid->init("MINU eID", PageIcon::Style { semiBold, "/images/my_eid_dark_38x38.png", "#ffffff", "#998B66" },
        PageIcon::Style { regular, "/images/my_eid_light_38x38.png", "#023664", "#ffffff" }, false );

    ui->cardInfo->fonts(regular, semiBold);
    ui->cardInfo->update("MARI MAASIKAS", "4845050123", "Lugejas on ID kaart");
}

MainWindow3::~MainWindow3()
{
    delete ui;
}
