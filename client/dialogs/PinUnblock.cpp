#include "PinUnblock.h"
#include "ui_PinUnblock.h"

PinUnblock::PinUnblock(QWidget *parent, PinDialog::PinFlags flags) :
    QDialog(parent),
    ui(new Ui::PinUnblock),
    regexpPUK("\\d{8,12}"),
    regexpPIN(),
    isPukOk(false),
    isPinOk(false),
    isRepeatOk(false)
{
    init(flags);
}

PinUnblock::~PinUnblock()
{
    delete ui;
}

void PinUnblock::init(PinDialog::PinFlags flags)
{
    ui->setupUi(this);
    setWindowFlags( Qt::Dialog | Qt::FramelessWindowHint );
    setWindowModality( Qt::ApplicationModal );

    ui->unblock->setEnabled(false);
    connect( ui->unblock, &QPushButton::clicked, this, &PinUnblock::accept );
    connect( ui->cancel, &QPushButton::clicked, this, &PinUnblock::reject );
    connect( this, &PinUnblock::finished, this, &PinUnblock::close );

    if(flags & PinDialog::Pin1Type)
    {
        ui->labelNameId->setText("PIN1 lahti blokeerimine");
        ui->label->setText(
                    "<span>"
                    "• PIN1 blokeeringu tühistamiseks sisesta kaardi PUK kood.<br>"
                    "• PUK koodi leiad ID-kaardi koodiümbrikus, kui sa pole seda vahepeal muutnud.<br>"
                    "• Uus PIN1 peab olema erinev eelmisest.<br>"
                    "• Kui sa ei tea oma ID-kaardi PUK koodi, külasta <u>klienditeeninduspunkti</u>,<br>"
                    "  kust saad uue koodiümbriku."
                    "</span>"
                    );
        ui->labelPin->setText("Uus PIN1 kood");
        ui->labelRepeat->setText("Uus PIN1 kood uuesti");

        regexpPIN.setPattern( "\\d{4,12}" );
    }
    else
    {
        ui->labelNameId->setText("PIN2 lahti blokeerimine");
        ui->label->setText(
                    "<span>"
                    "• PIN2 blokeeringu tühistamiseks sisesta kaardi PUK kood.<br>"
                    "• PUK koodi leiad ID-kaardi koodiümbrikus, kui sa pole seda vahepeal muutnud.<br>"
                    "• Uus PIN2 peab olema erinev eelmisest.<br>"
                    "• Kui sa ei tea oma ID-kaardi PUK koodi, külasta <u>klienditeeninduspunkti</u>,<br>"
                    "  kust saad uue koodiümbriku."
                    "</span>"
                    );
        ui->labelPin->setText("Uus PIN2 kood");
        ui->labelRepeat->setText("Uus PIN2 kood uuesti");

        regexpPIN.setPattern("\\d{5,12}");
    }

    ui->puk->setValidator(new QRegExpValidator(regexpPUK, ui->puk));
    ui->pin->setValidator(new QRegExpValidator(regexpPUK, ui->pin));
    ui->repeat->setValidator(new QRegExpValidator(regexpPUK, ui->repeat));

    ui->unblock->setStyleSheet(
            "border: none;"
            "padding: 6px 10px;"
            "border-radius: 3px;"
            "background-color: #97d7a8;"
            "font-family: Open Sans;"
            "font-size: 14px;"
            "color: #ffffff;"
            "font-weight: 400;"
            "line-height: 19px;"
            "text-align: center;"
                    );


    connect(ui->puk, &QLineEdit::textChanged, this,
                [this](const QString &text)
                {
                    isPukOk = regexpPUK.exactMatch(text);
                    setUnblockEnabled();
                }
            );
    connect(ui->pin, &QLineEdit::textChanged, this,
                [this](const QString &text)
                {
                    isPinOk = regexpPIN.exactMatch(text);
                    isRepeatOk = ui->pin->text() == ui->repeat->text();
                    setUnblockEnabled();
                }
            );
    connect(ui->repeat, &QLineEdit::textChanged, this,
                [this]()
                {
                    isRepeatOk = ui->pin->text() == ui->repeat->text();
                    setUnblockEnabled();
                }
            );

}

void PinUnblock::setUnblockEnabled()
{
    if(isPukOk)
    {
        ui->iconLabelPuk->setStyleSheet("image: url(:/images/ok.png);");
    }
    else
    {
        ui->iconLabelPuk->setStyleSheet("");
    }

    if(isPinOk)
    {
        ui->iconLabelPin->setStyleSheet("image: url(:/images/ok.png);");
    }
    else
    {
        ui->iconLabelPin->setStyleSheet("");
    }

    if(isRepeatOk)
    {
        ui->iconLabelRepeat->setStyleSheet("image: url(:/images/ok.png);");
    }
    else
    {
        ui->iconLabelRepeat->setStyleSheet("");
    }

    if(isPukOk && isPinOk && isRepeatOk)
    {
        ui->unblock->setEnabled(true);
        ui->unblock->setStyleSheet(
                "border: none;"
                "padding: 6px 10px;"
                "border-radius: 3px;"
                "background-color: #53c964;"
                "font-family: Open Sans;"
                "font-size: 14px;"
                "color: #ffffff;"
                "font-weight: 400;"
                "line-height: 19px;"
                "text-align: center;"
                );
    }
    else
    {
        ui->unblock->setEnabled(false);
        ui->unblock->setStyleSheet(
                "border: none;"
                "padding: 6px 10px;"
                "border-radius: 3px;"
                "background-color: #97d7a8;"
                "font-family: Open Sans;"
                "font-size: 14px;"
                "color: #ffffff;"
                "font-weight: 400;"
                "line-height: 19px;"
                "text-align: center;"
                );
    }
}
