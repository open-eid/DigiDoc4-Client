/*
 * QDigiDoc4
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "PinUnblock.h"
#include "ui_PinUnblock.h"
#include "effects/Overlay.h"
#include "Styles.h"


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

    ui->unblock->setEnabled( false );
    connect( ui->unblock, &QPushButton::clicked, this, &PinUnblock::accept );
    connect( ui->cancel, &QPushButton::clicked, this, &PinUnblock::reject );
    connect( this, &PinUnblock::finished, this, &PinUnblock::close );

    if(flags & PinDialog::Pin2Type)
    {
        ui->labelNameId->setText("PIN2 lahti blokeerimine");
        ui->label->setText(
                    "<ul>"
                        "<li>&nbsp;PIN2 blokeeringu tühistamiseks sisesta kaardi PUK kood.</li>"
                        "<li>&nbsp;PUK koodi leiad ID-kaarti koodiümbrikust, kui sa pole seda<br>&nbsp;vahepeal muutnud</li>"
                        "<li>&nbsp;Uus PIN2 peab olema erinev eelmisest.</li>"
                        "<li>&nbsp;Kui sa ei tea oma ID-kaardi PUK koodi, külasta<br>&nbsp;klienditeeninduspunkti, kust saad uue koodiümbriku.</li>"
                    "</ul>"
                    );
        ui->labelPin->setText("UUS PIN2 KOOD");
        ui->labelRepeat->setText("UUS PIN2 KOOD UUESTI");

        regexpPIN.setPattern("\\d{5,12}");
    }
    else
    {
        ui->labelNameId->setText("PIN1 lahti blokeerimine");
        ui->label->setText(
                    "<ul>"
                        "<li>&nbsp;PIN1 blokeeringu tühistamiseks sisesta kaardi PUK kood.</li>"
                        "<li>&nbsp;PUK koodi leiad ID-kaarti koodiümbrikust, kui sa pole seda<br>&nbsp;vahepeal muutnud</li>"
                        "<li>&nbsp;Uus PIN1 peab olema erinev eelmisest.</li>"
                        "<li>&nbsp;Kui sa ei tea oma ID-kaardi PUK koodi, külasta<br>&nbsp;klienditeeninduspunkti, kust saad uue koodiümbriku.</li>"
                    "</ul>"
                    );
        ui->labelPin->setText("UUS PIN1 KOOD");
        ui->labelRepeat->setText("UUS PIN1 KOOD UUESTI");

        regexpPIN.setPattern( "\\d{4,12}" );
    }

    ui->puk->setValidator(new QRegExpValidator(regexpPUK, ui->puk));
    ui->pin->setValidator(new QRegExpValidator(regexpPUK, ui->pin));
    ui->repeat->setValidator(new QRegExpValidator(regexpPUK, ui->repeat));

    QFont condensed14 = Styles::font( Styles::Condensed, 14 );
    QFont headerFont( Styles::font( Styles::Regular, 18 ) );
    headerFont.setWeight( QFont::Bold );
    ui->labelNameId->setFont( headerFont );
    ui->unblock->setFont( condensed14 );
    ui->label->setFont( Styles::font( Styles::Regular, 14 ) );
    ui->labelPuk->setFont( Styles::font( Styles::Condensed, 12 ) );

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
        ui->iconLabelPuk->setStyleSheet("image: url(:/images/icon_check.svg);");
    }
    else
    {
        ui->iconLabelPuk->setStyleSheet("");
    }

    if(isPinOk)
    {
        ui->iconLabelPin->setStyleSheet("image: url(:/images/icon_check.svg);");
    }
    else
    {
        ui->iconLabelPin->setStyleSheet("");
    }

    if(isRepeatOk)
    {
        ui->iconLabelRepeat->setStyleSheet("image: url(:/images/icon_check.svg);");
    }
    else
    {
        ui->iconLabelRepeat->setStyleSheet("");
    }

    ui->unblock->setEnabled( isPukOk && isPinOk && isRepeatOk );
}

int PinUnblock::exec()
{
    Overlay overlay(parentWidget());
    overlay.show();
    auto rc = QDialog::exec();
    overlay.close();

    return rc;
}
