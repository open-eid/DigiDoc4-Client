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

#include <QLabel>
#include <QList>
#include <QtGui/QRegExpValidator>


PinUnblock::PinUnblock(WorkMode mode, QWidget *parent, QSmartCardData::PinType type, short leftAttempts,
	QDate birthDate, QString personalCode)
	: QDialog(parent)
	, ui(new Ui::PinUnblock)
	, birthDate(birthDate)
	, personalCode(std::move(personalCode))
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

	QFont condensed12 = Styles::font(Styles::Condensed, 12);
	ui->labelPuk->setFont(condensed12);
	ui->labelPin->setFont(condensed12);
	ui->labelRepeat->setFont(condensed12);

	if( mode == PinUnblock::ChangePinWithPuk )
	{
		ui->labelNameId->setText( tr("%1 code change").arg( QSmartCardData::typeString( type ) ) );
		regexpFirstCode.setPattern(QStringLiteral("\\d{8,12}"));
		regexpNewCode.setPattern((type == QSmartCardData::Pin1Type) ? QStringLiteral("\\d{4,12}") : QStringLiteral("\\d{5,12}"));
		ui->unblock->setText( tr("CHANGE") );
	}
	if( mode == PinUnblock::UnBlockPinWithPuk )
	{
		ui->labelNameId->setText( tr("%1 unblocking").arg( QSmartCardData::typeString( type ) ) );
		regexpFirstCode.setPattern(QStringLiteral("\\d{8,12}"));
		regexpNewCode.setPattern((type == QSmartCardData::Pin1Type) ? QStringLiteral("\\d{4,12}") : QStringLiteral("\\d{5,12}"));
	}
	else if( mode == PinUnblock::PinChange )
	{
		ui->labelNameId->setText( tr("%1 code change").arg( QSmartCardData::typeString( type ) ) );
		ui->labelPuk->setText( tr( "VALID %1 CODE").arg( QSmartCardData::typeString( type ) ) );
		regexpFirstCode.setPattern((type == QSmartCardData::Pin1Type) ? QStringLiteral("\\d{4,12}") :
			(type == QSmartCardData::Pin2Type) ? QStringLiteral("\\d{5,12}") : QStringLiteral("\\d{8,12}"));
		regexpNewCode.setPattern((type == QSmartCardData::Pin1Type) ? QStringLiteral("\\d{4,12}") :
			(type == QSmartCardData::Pin2Type) ? QStringLiteral("\\d{5,12}") : QStringLiteral("\\d{8,12}"));
		ui->unblock->setText( tr("CHANGE") );
	}
	setWindowTitle(ui->labelNameId->text());
	ui->labelPin->setText( tr( "NEW %1 CODE").arg( QSmartCardData::typeString( type ) ) );
	ui->labelRepeat->setText( tr( "NEW %1 CODE AGAIN").arg( QSmartCardData::typeString( type ) ) );
	ui->pin->setAccessibleName(ui->labelPin->text().toLower());
	ui->repeat->setAccessibleName(ui->labelRepeat->text().toLower());
	ui->puk->setAccessibleName(ui->labelPuk->text().toLower());
	ui->unblock->setAccessibleName(ui->unblock->text().toLower());

	ui->puk->setValidator( new QRegExpValidator( regexpFirstCode, ui->puk ) );
	ui->pin->setValidator( new QRegExpValidator( regexpFirstCode, ui->pin ) );
	ui->repeat->setValidator( new QRegExpValidator( regexpFirstCode, ui->repeat ) );

	QFont condensed14(Styles::font(Styles::Condensed, 14));
	QFont headerFont(Styles::font(Styles::Regular, 18));
	QFont regular13(Styles::font(Styles::Regular, 13));
	headerFont.setWeight( QFont::Bold );
	ui->labelNameId->setFont( headerFont );
	ui->cancel->setFont( condensed14 );
	ui->unblock->setFont( condensed14 );
	ui->labelAttemptsLeft->setFont(regular13);
	ui->labelPinValidation->setFont(regular13);
	ui->labelPinValidation->hide();
	if( mode == PinUnblock::ChangePinWithPuk || mode == PinUnblock::UnBlockPinWithPuk )
		ui->labelAttemptsLeft->setText( tr("PUK remaining attempts: %1").arg( leftAttempts ) );
	else
		ui->labelAttemptsLeft->setText( tr("Remaining attempts: %1").arg( leftAttempts ) );
	ui->labelAttemptsLeft->setVisible( leftAttempts < 3 );

	initIntro(mode, type);

	connect(ui->puk, &QLineEdit::textChanged, this, [this](const QString &text) {
		isFirstCodeOk = regexpFirstCode.exactMatch(text);
		setUnblockEnabled();
	});
	connect(ui->pin, &QLineEdit::textChanged, this, [this, type, mode](const QString &text) {
		ui->labelPinValidation->hide();
		isNewCodeOk = regexpNewCode.exactMatch(text) && validatePin(text, type, mode);
		isRepeatCodeOk = !text.isEmpty() && ui->pin->text() == ui->repeat->text();
		setUnblockEnabled();
	});
	connect(ui->repeat, &QLineEdit::textChanged, this, [this] {
		isRepeatCodeOk = !ui->pin->text().isEmpty() && ui->pin->text() == ui->repeat->text();
		setUnblockEnabled();
	});
}

void PinUnblock::initIntro(WorkMode mode, QSmartCardData::PinType type)
{
	switch(mode)
	{
	case PinUnblock::ChangePinWithPuk:
		ui->line1_text->setText(type == QSmartCardData::Pin2Type
				? tr("PIN2 code is used to digitally sign documents.")
				: tr("PIN1 code is used for confirming the identity of a person."));
		ui->line2_text->setText(tr("If you have forgotten PIN%1, but know PUK, then here you can enter new PIN%1.").arg(type));
		ui->line3_text->setText(tr("PUK code is written in the envelope, that is given with the ID-card."));
		break;
	case PinUnblock::UnBlockPinWithPuk:
		ui->line1_text->setText(tr("To unblock the certificate you have to enter the PUK code."));
		ui->line2_text->setText(tr("You can find your PUK code inside the ID-card codes envelope."));
		ui->line3_text->setText(tr("If you have forgotten the PUK code for your ID card, please visit "
				"<a href=\"https://www.politsei.ee/en/\"><span style=\"color: #006EB5; text-decoration: none;\">"
				"the Police and Border Guard Board service center</span></a> to obtain new PIN codes."));
		break;
	default:
		if(type == QSmartCardData::Pin2Type ||  type == QSmartCardData::Pin1Type)
		{
			ui->line1_text->setText(type == QSmartCardData::Pin2Type ?
					tr("PIN2 code is used to digitally sign documents.") :
					tr("PIN1 code is used for confirming the identity of a person."));
			ui->line2_text->setText(
				tr("If PIN%1 is inserted incorrectly 3 times the %2 certificate will be blocked and it will be impossible to use ID-card to %3, until it is unblocked via the PUK code.")
					.arg(type)
					.arg(type == QSmartCardData::Pin2Type ? tr("signing") : tr("identification"))
					.arg(type == QSmartCardData::Pin2Type ? tr("digital signing") : tr("verify identification"))
			);
		}
		else if(type == QSmartCardData::PukType)
		{
			ui->line1_text->setText(tr("PUK code is used for unblocking the certificates, when PIN1 or PIN2 has been entered 3 times incorrectly."));
			ui->line2_text->setText(tr("If you forget the PUK code or the certificates remain blocked, you have to visit the <a href=\"https://www.politsei.ee/en/\">"
					"<span style=\"color: #006EB5; text-decoration: none;\">service center</span></a> to obtain new codes."));
		}
		break;
	}
	QFont font = Styles::font(Styles::Regular, 14);
	for(int i = 0; i < 4; i++)
	{
		bool isHidden = true;
		if(QLabel *text = findChild<QLabel*>(QStringLiteral("line%1_text").arg(i + 1)))
		{
			text->setFont(font);
			text->setHidden(isHidden = text->text().isEmpty());
		}
		if(QLabel *bullet = findChild<QLabel*>(QStringLiteral("line%1_bullet").arg(i + 1)))
		{
			bullet->setFont(font);
			bullet->setHidden(isHidden);
		}
	}
}

void PinUnblock::setError(const QString &msg)
{
	ui->labelPinValidation->setHidden(msg.isEmpty());
	ui->labelPinValidation->setText(msg);
}

void PinUnblock::setUnblockEnabled()
{
	ui->iconLabelPuk->load(isFirstCodeOk ? QStringLiteral(":/images/icon_check.svg") : QString());
	ui->iconLabelPin->load(isNewCodeOk ? QStringLiteral(":/images/icon_check.svg") : QString());
	ui->iconLabelRepeat->load(isRepeatCodeOk ? QStringLiteral(":/images/icon_check.svg") : QString());
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

bool PinUnblock::validatePin(const QString& pin, QSmartCardData::PinType type, WorkMode mode)
{
	const static QString SEQUENCE_ASCENDING = QStringLiteral("1234567890123456789012");
	const static QString SEQUENCE_DESCENDING = QStringLiteral("0987654321098765432109");
	QString pinType = type == QSmartCardData::Pin1Type ? QStringLiteral("PIN1") : (type == QSmartCardData::Pin2Type ? QStringLiteral("PIN2") : QStringLiteral("PUK"));

	if(mode == PinUnblock::PinChange && pin == ui->puk->text())
	{
		setError(tr("Current %1 code and new %1 code must be different").arg(pinType));
		return false;
	}
	if(SEQUENCE_ASCENDING.contains(pin))
	{
		setError(tr("New %1 code can't be increasing sequence").arg(pinType));
		return false;
	}
	if(SEQUENCE_DESCENDING.contains(pin))
	{
		setError(tr("New %1 code can't be decreasing sequence").arg(pinType));
		return false;
	}
	if(pin.count(pin[1]) == pin.length())
	{
		setError(tr("New %1 code can't be sequence of same numbers").arg(pinType));
		return false;
	}
	if(personalCode.contains(pin))
	{
		setError(tr("New %1 code can't be part of your personal code").arg(pinType));
		return false;
	}
	if(pin == birthDate.toString(QStringLiteral("yyyy")) || pin == birthDate.toString(QStringLiteral("ddMM")) || pin == birthDate.toString(QStringLiteral("MMdd")) ||
		 pin == birthDate.toString(QStringLiteral("yyyyMMdd")) || pin == birthDate.toString(QStringLiteral("ddMMyyyy")))
	{
		setError(tr("New %1 code can't be your date of birth").arg(pinType));
		return false;
	}

	return true;
}
