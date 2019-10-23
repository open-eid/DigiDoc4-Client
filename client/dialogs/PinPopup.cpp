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
#include "SslCertificate.h"
#include "effects/Overlay.h"
#include "dialogs/WaitDialog.h"

#include <common/Common.h>

#include <QtCore/QTimeLine>
#include <QtGui/QMovie>
#include <QtGui/QRegExpValidator>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>

PinPopup::PinPopup(PinFlags flags, const TokenData &t, QWidget *parent)
	: PinPopup(flags, t.cert(), t.flags(), parent)
{
}

PinPopup::PinPopup(PinFlags flags, const SslCertificate &c, TokenData::TokenFlags token, QWidget *parent)
	: PinPopup(flags, c.toString(c.showCN() ? QStringLiteral("<b>CN,</b> serialNumber") : QStringLiteral("<b>GN SN,</b> serialNumber")), token, parent)
{
	if(c.type() & SslCertificate::TempelType)
	{
		regexp.setPattern(QStringLiteral(".{4,}"));
		ui->pin->setValidator(new QRegExpValidator(regexp, ui->pin));
		ui->pin->setMaxLength(32767);
	}
}

PinPopup::PinPopup(PinFlags flags, const QString &title, TokenData::TokenFlags token, QWidget *parent, const QString &bodyText)
	: QDialog(parent)
	, ui(new Ui::PinPopup)
{
	ui->setupUi(this);
	setWindowFlags( Qt::Dialog | Qt::FramelessWindowHint );
	setWindowModality( Qt::ApplicationModal );
	setFixedSize( size() );
	Overlay *overlay = new Overlay(parent->topLevelWidget());
	overlay->show();
	connect(this, &PinPopup::destroyed, overlay, &Overlay::deleteLater);

	QFont regular = Styles::font( Styles::Regular, 13 );
	QFont condensed14 = Styles::font( Styles::Condensed, 14 );
	
	ui->labelNameId->setFont( Styles::font( Styles::Regular, 14 ) );
	ui->label->setFont( regular );
	ui->ok->setFont( condensed14 );
	ui->cancel->setFont( condensed14 );
	ui->ok->setEnabled( false );

	connect( ui->ok, &QPushButton::clicked, this, &PinPopup::accept );
	connect( ui->cancel, &QPushButton::clicked, this, &PinPopup::reject );
	connect( this, &PinPopup::finished, this, &PinPopup::close );

	QString text;
	
	if(!bodyText.isEmpty())
	{
		text = bodyText;
	}
	else
	{
		if( token & TokenData::PinFinalTry )
			text += QStringLiteral("<font color='red'><b>%1</b></font><br />").arg(tr("PIN will be locked next failed attempt"));
		else if( token & TokenData::PinCountLow )
			text += QStringLiteral("<font color='red'><b>%1</b></font><br />").arg(tr("PIN has been entered incorrectly at least once"));

		if( flags & Pin2Type )
		{
			QString t = flags & PinpadFlag ?
				tr("For using sign certificate enter PIN2 at the reader") :
				tr("For using sign certificate enter PIN2");
			text += tr("Selected action requires sign certificate.") + QStringLiteral("<br />") + t;
			setPinLen(5);
		}
		else
		{
			QString t = flags & PinpadFlag ?
				tr("For using authentication certificate enter PIN1 at the reader") :
				tr("For using authentication certificate enter PIN1");
			text += tr("Selected action requires authentication certificate.") + QStringLiteral("<br />") + t;
			setPinLen(4);
		}
	}
	ui->labelNameId->setText(QStringLiteral("<b>%1</b>").arg(title));
	ui->label->setText( text );
	Common::setAccessibleName( ui->label );

	if(flags & PinpadChangeFlag)
	{
		ui->pin->hide();
		ui->ok->hide();
		ui->cancel->hide();
		QLabel *movie = new QLabel(this);
		movie->setAlignment(Qt::AlignCenter);
		movie->setMovie(new QMovie(QStringLiteral(":/images/wait.gif"), QByteArray(), movie));
		movie->movie()->setScaledSize(QSize(movie->height(), movie->height()));
		movie->move(ui->pin->pos());
		movie->resize(ui->pin->size());
		movie->show();
		movie->movie()->start();
	}
	if( flags & PinpadFlag )
	{
		ui->pin->hide();
		ui->ok->hide();
		ui->cancel->hide();
		QProgressBar *progress = new QProgressBar( this );
		progress->setRange( 0, 30 );
		progress->setValue( progress->maximum() );
		progress->setTextVisible( false );
		progress->resize( 200, 30 );
		progress->move( 153, 122 );
		QTimeLine *statusTimer = new QTimeLine( progress->maximum() * 1000, this );
		statusTimer->setCurveShape( QTimeLine::LinearCurve );
		statusTimer->setFrameRange( progress->maximum(), progress->minimum() );
		connect( statusTimer, &QTimeLine::frameChanged, progress, &QProgressBar::setValue );
		connect( this, &PinPopup::startTimer, statusTimer, &QTimeLine::start );
	}
	else if( !(flags & PinpadNoProgressFlag) )
	{
		ui->pin->setFocus();
		ui->pin->setValidator( new QRegExpValidator( regexp, ui->pin ) );
		ui->pin->setMaxLength( 12 );
		connect(ui->pin, &QLineEdit::textEdited, this, [&](const QString &text) {
			ui->ok->setEnabled(regexp.exactMatch(text));
		});
		ui->label->setBuddy( ui->pin );
		ui->ok->setDisabled(true);
	}
}

PinPopup::~PinPopup()
{
	delete ui;
}

void PinPopup::setPinLen(unsigned long minLen, unsigned long maxLen)
{
	QString charPattern = regexp.pattern().startsWith('.') ? QStringLiteral(".") : QStringLiteral("\\d");
	regexp.setPattern(QStringLiteral("%1{%2,%3}").arg(charPattern).arg(minLen).arg(maxLen));
}

QString PinPopup::text() const { return ui->pin->text(); }
