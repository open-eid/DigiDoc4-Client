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

#include "PinPopup.h"
#include "ui_PinPopup.h"
#include "Styles.h"
#include "effects/Overlay.h"

#include <common/Common.h>
#include <common/SslCertificate.h>

#include <QtCore/QTimeLine>
#include <QtGui/QRegExpValidator>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>

PinPopup::PinPopup( PinDialog::PinFlags flags, const TokenData &t, QWidget *parent )
: QDialog(parent)
, ui(new Ui::PinPopup)
{
    SslCertificate c = t.cert();
	init( flags, c.toString( c.showCN() ? "<b>CN,</b> serialNumber" : "<b>GN SN,</b> serialNumber" ), t.flags() );
}

PinPopup::PinPopup( PinDialog::PinFlags flags, const QSslCertificate &cert, TokenData::TokenFlags token, QWidget *parent )
: QDialog(parent)
, ui(new Ui::PinPopup)
{
	SslCertificate c = cert;
	init( flags, c.toString( c.showCN() ? "<b>CN,</b> serialNumber" : "<b>GN SN,</b> serialNumber" ), token );
}

PinPopup::PinPopup( PinDialog::PinFlags flags, const QString &title, TokenData::TokenFlags token, QWidget *parent )
: QDialog(parent)
, ui(new Ui::PinPopup)
{
    init( flags, title, token );
}

PinPopup::~PinPopup()
{
    delete ui;
}

void PinPopup::init( PinDialog::PinFlags flags, const QString &title, TokenData::TokenFlags token )
{
    ui->setupUi(this);
    setWindowFlags( Qt::Dialog | Qt::FramelessWindowHint );
    setWindowModality( Qt::ApplicationModal );

    QFont openSansRegular13 = Styles::instance().font( Styles::OpenSansRegular, 13 );
    QFont openSansRegular14 = Styles::instance().font( Styles::OpenSansRegular, 14 );
    
    ui->labelNameId->setFont( openSansRegular14 );
    ui->label->setFont( openSansRegular13 );
    ui->ok->setFont( openSansRegular14 );
    ui->cancel->setFont( openSansRegular14 );

    connect( ui->ok, &QPushButton::clicked, this, &PinPopup::accept );
    connect( ui->cancel, &QPushButton::clicked, this, &PinPopup::reject );
    connect( this, &PinPopup::finished, this, &PinPopup::close );

    QString text;
    
    if( token & TokenData::PinFinalTry )
        text += "<font color='red'><b>" + tr("PIN will be locked next failed attempt") + "</b></font><br />";
    else if( token & TokenData::PinCountLow )
        text += "<font color='red'><b>" + tr("PIN has been entered incorrectly one time") + "</b></font><br />";

    ui->labelNameId->setText( QString( "<b>%1</b>" ).arg( title ) );
    if( flags & PinDialog::Pin2Type )
    {
        QString t = flags & PinDialog::PinpadFlag ?
            tr("For using sign certificate enter PIN2 at the reader") :
            tr("For using sign certificate enter PIN2");
        text += tr("Selected action requires sign certificate.") + "<br />" + t;
        regexp.setPattern( "\\d{5,12}" );
    }
    else
    {
        QString t = flags & PinDialog::PinpadFlag ?
            tr("For using authentication certificate enter PIN1 at the reader") :
            tr("For using authentication certificate enter PIN1");
        text += tr("Selected action requires authentication certificate.") + "<br />" + t;
        regexp.setPattern( "\\d{4,12}" );
    }
    ui->label->setText( text );
    Common::setAccessibleName( ui->label );

    if( flags & PinDialog::PinpadFlag )
	{
        ui->pin->hide();
        ui->ok->hide();
        ui->cancel->hide();
		QProgressBar *progress = new QProgressBar( this );
		progress->setRange( 0, 30 );
		progress->setValue( progress->maximum() );
        progress->setTextVisible( false );
        progress->resize( 200, 30 );
		progress->move( 153, 106 );
		QTimeLine *statusTimer = new QTimeLine( progress->maximum() * 1000, this );
		statusTimer->setCurveShape( QTimeLine::LinearCurve );
		statusTimer->setFrameRange( progress->maximum(), progress->minimum() );
		connect( statusTimer, &QTimeLine::frameChanged, progress, &QProgressBar::setValue );
		connect( this, &PinPopup::startTimer, statusTimer, &QTimeLine::start );
	}
	else if( !(flags & PinDialog::PinpadNoProgressFlag) )
	{
		ui->pin->setFocus();
		ui->pin->setValidator( new QRegExpValidator( regexp, ui->pin ) );
		ui->pin->setMaxLength( 12 );
		connect( ui->pin, &QLineEdit::textEdited, this, &PinPopup::textEdited );
		ui->label->setBuddy( ui->pin );

		textEdited( QString() );
	}
}

QString PinPopup::text() const { return ui->pin->text(); }

void PinPopup::textEdited( const QString &text )
{ 
    ui->ok->setEnabled( regexp.exactMatch( text ) ); 
}

int PinPopup::exec()
{ 
    Overlay overlay(parentWidget());
    overlay.show();
    auto rc = QDialog::exec();
    overlay.close();

    return rc;
}
