#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "Styles.h"

#include <QDebug>

MainWindow::MainWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    qDebug() << "Set fonts";
    const QFont& regular = Styles::instance().fontRegular();
    const QFont& semiBold = Styles::instance().fontSemiBold();
    ui->signature->init( "ALLKIRI", PageIcon::Style { semiBold, "/images/sign_dark_38x38.png", "#ffffff", "#998B66" },
        PageIcon::Style { regular, "/images/sign_light_38x38.png", "#023664", "#ffffff" }, true );
    ui->crypto->init( "KRÜPTO", PageIcon::Style { semiBold, "/images/crypto_dark_38x38.png", "#ffffff", "#998B66" },
        PageIcon::Style { regular, "/images/crypto_light_38x38.png", "#023664", "#ffffff" }, false );
    ui->myEid->init("MINU eID", PageIcon::Style { semiBold, "/images/my_eid_dark_38x38.png", "#ffffff", "#998B66" },
        PageIcon::Style { regular, "/images/my_eid_light_38x38.png", "#023664", "#ffffff" }, false );

    ui->cardInfo->fonts(regular, semiBold);
    ui->cardInfo->update("MARI MAASIKAS", "4845050123", "Lugejas on ID kaart");

    connect( ui->signature, SIGNAL(activated( PageIcon *const )), SLOT(pageSelected( PageIcon *const )) );
    connect( ui->crypto, SIGNAL(activated( PageIcon *const )), SLOT(pageSelected( PageIcon *const )) );
    connect( ui->myEid, SIGNAL(activated( PageIcon *const )), SLOT(pageSelected( PageIcon *const )) );
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::pageSelected( PageIcon *const page )
{
    if( page != ui->signature ) {
        ui->signature->select(false);
    }
    if( page != ui->crypto ) {
        ui->crypto->select(false);
    }
    if( page != ui->myEid ) {
        ui->myEid->select(false);
    }
}
