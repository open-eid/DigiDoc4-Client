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

#include "Styles.h"
#include "effects/Overlay.h"


PinUnblock::PinUnblock( WorkMode mode, QWidget *parent, QSmartCardData::PinType type, short leftAttempts ) :
	QDialog( parent ),
	ui( new Ui::PinUnblock ),
	regexpFirstCode(),
	regexpNewCode(),
	isFirstCodeOk( false ),
	isNewCodeOk( false ),
	isRepeatCodeOk( false )
{
	init( mode, type, leftAttempts );
	adjustSize();
	setFixedSize( size() );
}

PinUnblock::~PinUnblock()
{
	delete ui;
}

void PinUnblock::init( WorkMode mode, QSmartCardData::PinType type, short leftAttempts )
{
	ui->setupUi( this );
	setWindowFlags( Qt::Dialog | Qt::FramelessWindowHint );
	setWindowModality( Qt::ApplicationModal );

	ui->unblock->setEnabled( false );
	connect( ui->unblock, &QPushButton::clicked, this, &PinUnblock::accept );
	connect( ui->cancel, &QPushButton::clicked, this, &PinUnblock::reject );
	connect( this, &PinUnblock::finished, this, &PinUnblock::close );

	if( mode == PinUnblock::ChangePinWithPuk )
	{
		if( type == QSmartCardData::Pin2Type )
		{
			ui->label->setText(tr("ConditionsChangePIN2WithPUK"));
		}
		else if( type == QSmartCardData::Pin1Type ) 
		{
			ui->label->setText(tr("ConditionsChangePIN1WithPUK"));
		}
		ui->labelNameId->setText( tr("%1 code change").arg( QSmartCardData::typeString( type ) ) );
		regexpFirstCode.setPattern( "\\d{8,12}" );
		regexpNewCode.setPattern( (type == QSmartCardData::Pin1Type) ? "\\d{4,12}" : "\\d{5,12}" );
		ui->unblock->setText( tr("CHANGE") );
	}
	if( mode == PinUnblock::UnBlockPinWithPuk )
	{
		if( type == QSmartCardData::Pin2Type )
		{
			ui->label->setText(tr("ConditionsUnlockPIN2"));
		}
		else if( type == QSmartCardData::Pin1Type )
		{
			ui->label->setText(tr("ConditionsUnlockPIN1"));
		}
		ui->labelNameId->setText( tr("%1 unblocking").arg( QSmartCardData::typeString( type ) ) );
		regexpFirstCode.setPattern( "\\d{8,12}" );
		regexpNewCode.setPattern( (type == QSmartCardData::Pin1Type) ? "\\d{4,12}" : "\\d{5,12}" );
	}
	else if( mode == PinUnblock::PinChange )
	{
		if( type == QSmartCardData::Pin2Type )
		{
			ui->label->setText(tr("ConditionsChangePIN2"));
		}
		else if( type == QSmartCardData::Pin1Type )
		{
			ui->label->setText(tr("ConditionsChangePIN1"));
		}
		else if( type == QSmartCardData::PukType )
		{
			ui->label->setText(tr("ConditionsChangePUK"));
		}
		ui->labelNameId->setText( tr("%1 code change").arg( QSmartCardData::typeString( type ) ) );
		ui->labelPuk->setText( tr( "VALID %1 CODE").arg( QSmartCardData::typeString( type ) ) );
		regexpFirstCode.setPattern( (type == QSmartCardData::Pin1Type) ? "\\d{4,12}" :
			(type == QSmartCardData::Pin2Type) ? "\\d{5,12}" : "\\d{8,12}" );
		regexpNewCode.setPattern( (type == QSmartCardData::Pin1Type) ? "\\d{4,12}" :
			(type == QSmartCardData::Pin2Type) ? "\\d{5,12}" : "\\d{8,12}" );
		ui->unblock->setText( tr("CHANGE") );
	}
	ui->labelPin->setText( tr( "NEW %1 CODE").arg( QSmartCardData::typeString( type ) ) );
	ui->labelRepeat->setText( tr( "NEW %1 CODE AGAIN").arg( QSmartCardData::typeString( type ) ) );

	ui->puk->setValidator( new QRegExpValidator( regexpFirstCode, ui->puk ) );
	ui->pin->setValidator( new QRegExpValidator( regexpFirstCode, ui->pin ) );
	ui->repeat->setValidator( new QRegExpValidator( regexpFirstCode, ui->repeat ) );

	QFont condensed14 = Styles::font( Styles::Condensed, 14 );
	QFont headerFont( Styles::font( Styles::Regular, 18 ) );
	headerFont.setWeight( QFont::Bold );
	ui->labelNameId->setFont( headerFont );
	ui->cancel->setFont( condensed14 );
	ui->unblock->setFont( condensed14 );
#if defined(Q_OS_MAC)
	ui->label->setFont( Styles::font( Styles::Regular, 13 ) );
#else
	ui->label->setFont( Styles::font( Styles::Regular, 14 ) );
#endif
	ui->labelPuk->setFont( Styles::font( Styles::Condensed, 12 ) );
	ui->labelAttemptsLeft->setFont( Styles::font( Styles::Regular, 13 ) );
	if( mode == PinUnblock::ChangePinWithPuk || mode == PinUnblock::UnBlockPinWithPuk )
		ui->labelAttemptsLeft->setText( tr("PUK remaining attempts: %1").arg( leftAttempts ) );
	else
		ui->labelAttemptsLeft->setText( tr("Remaining attempts: %1").arg( leftAttempts ) );
	ui->labelAttemptsLeft->setVisible( leftAttempts < 3 );

	connect(ui->puk, &QLineEdit::textChanged, this,
				[this](const QString &text)
				{
					isFirstCodeOk = regexpFirstCode.exactMatch(text);
					setUnblockEnabled();
				}
			);
	connect(ui->pin, &QLineEdit::textChanged, this,
				[this](const QString &text)
				{
					isNewCodeOk = regexpNewCode.exactMatch(text);
					isRepeatCodeOk = ui->pin->text() == ui->repeat->text();
					setUnblockEnabled();
				}
			);
	connect(ui->repeat, &QLineEdit::textChanged, this,
				[this]()
				{
					isRepeatCodeOk = ui->pin->text() == ui->repeat->text();
					setUnblockEnabled();
				}
			);
}

void PinUnblock::setUnblockEnabled()
{
	if( isFirstCodeOk )
	{
		ui->iconLabelPuk->setStyleSheet("image: url(:/images/icon_check.svg);");
	}
	else
	{
		ui->iconLabelPuk->setStyleSheet("");
	}

	if( isNewCodeOk )
	{
		ui->iconLabelPin->setStyleSheet("image: url(:/images/icon_check.svg);");
	}
	else
	{
		ui->iconLabelPin->setStyleSheet("");
	}

	if( isRepeatCodeOk )
	{
		ui->iconLabelRepeat->setStyleSheet("image: url(:/images/icon_check.svg);");
	}
	else
	{
		ui->iconLabelRepeat->setStyleSheet("");
	}

	ui->unblock->setEnabled( isFirstCodeOk && isNewCodeOk && isRepeatCodeOk );
}

int PinUnblock::exec()
{
	Overlay overlay( parentWidget() );
	overlay.show();
	auto rc = QDialog::exec();
	overlay.close();

	return rc;
}

QString PinUnblock::firstCodeText() const { return ui->puk->text(); }

QString PinUnblock::newCodeText() const { return ui->pin->text(); }
