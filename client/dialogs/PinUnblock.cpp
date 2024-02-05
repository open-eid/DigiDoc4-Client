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

#include <QtGui/QRegularExpressionValidator>

PinUnblock::PinUnblock(WorkMode mode, QWidget *parent, QSmartCardData::PinType type,
	short leftAttempts, QDate birthDate, const QString &personalCode)
	: QDialog(parent)
	, ui(new Ui::PinUnblock)
{
	ui->setupUi(this);
#if defined (Q_OS_WIN)
	ui->buttonLayout->setDirection(QBoxLayout::RightToLeft);
#endif
	setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint);
	auto list = findChildren<QLineEdit*>();
	if(!list.isEmpty())
		list.first()->setFocus();
	for(QLineEdit *w: list)
		w->setAttribute(Qt::WA_MacShowFocusRect, false);
	new Overlay(this);

	QFont condensed14 = Styles::font(Styles::Condensed, 14);
	QFont condensed12 = Styles::font(Styles::Condensed, 12);
	QFont regular14 = Styles::font(Styles::Regular, 14);
	QFont regular12 = Styles::font(Styles::Regular, 12);
	ui->errorPuk->setFont(regular12);
	ui->errorPin->setFont(regular12);
	ui->errorRepeat->setFont(regular12);
	ui->labelPuk->setFont(condensed12);
	ui->labelPin->setFont(condensed12);
	ui->labelRepeat->setFont(condensed12);
	ui->labelNameId->setFont(Styles::font(Styles::Regular, 20, QFont::DemiBold));
	ui->cancel->setFont(condensed14);
	ui->change->setFont(condensed14);
	for(QLabel *text: findChildren<QLabel*>(QRegularExpression(QStringLiteral("line\\d_[text,bullet]"))))
		text->setFont(regular14);

	auto pattern = [](QSmartCardData::PinType type) {
		return QStringLiteral("^\\d{%1,12}$").arg(QSmartCardData::minPinLen(type));
	};

	QRegularExpression regexpValidateCode;
	QRegularExpression regexpNewCode;
	regexpNewCode.setPattern(pattern(type));
	switch(mode)
	{
	case PinUnblock::UnBlockPinWithPuk:
		ui->labelNameId->setText(tr("%1 unblocking").arg(QSmartCardData::typeString(type)));
		ui->change->setText(tr("UNBLOCK"));
		regexpValidateCode.setPattern(pattern(QSmartCardData::PukType));
		ui->line1_text->setText(tr("To unblock the certificate you have to enter the PUK code."));
		ui->line2_text->setText(tr("You can find your PUK code inside the ID-card codes envelope."));
		ui->line3_text->setText(tr("If you have forgotten the PUK code for your ID card, please visit "
								   "<a href=\"https://www.politsei.ee/en/\"><span style=\"color: #006EB5; text-decoration: none;\">"
								   "the Police and Border Guard Board service center</span></a> to obtain new PIN codes."));
		break;
	case PinUnblock::ChangePinWithPuk:
		ui->labelNameId->setText(tr("%1 code change").arg(QSmartCardData::typeString(type)));
		regexpValidateCode.setPattern(pattern(QSmartCardData::PukType));
		ui->line1_text->setText(type == QSmartCardData::Pin2Type
									? tr("PIN2 code is used to digitally sign documents.")
									: tr("PIN1 code is used for confirming the identity of a person."));
		ui->line2_text->setText(tr("If you have forgotten PIN%1, but know PUK, then here you can enter new PIN%1.").arg(type));
		ui->line3_text->setText(tr("PUK code is written in the envelope, that is given with the ID-card."));
		break;
	case PinUnblock::PinChange:
		ui->labelNameId->setText(tr("%1 code change").arg(QSmartCardData::typeString(type)));
		ui->labelPuk->setText(tr("VALID %1 CODE").arg(QSmartCardData::typeString(type)));
		regexpValidateCode.setPattern(pattern(type));
		if(type == QSmartCardData::PukType)
		{
			ui->line1_text->setText(tr("PUK code is used for unblocking the certificates, when PIN1 or PIN2 has been entered 3 times incorrectly."));
			ui->line2_text->setText(tr("If you forget the PUK code or the certificates remain blocked, you have to visit the <a href=\"https://www.politsei.ee/en/\">"
									   "<span style=\"color: #006EB5; text-decoration: none;\">service center</span></a> to obtain new codes."));
			break;
		}
		ui->line1_text->setText(type == QSmartCardData::Pin2Type
									? tr("PIN2 code is used to digitally sign documents.")
									: tr("PIN1 code is used for confirming the identity of a person."));
		ui->line2_text->setText(
			tr("If %1 is inserted incorrectly 3 times the %2 certificate will be blocked and it will be impossible to use ID-card to %3, until it is unblocked via the PUK code.").arg(
				QSmartCardData::typeString(type),
				type == QSmartCardData::Pin2Type ? tr("signing") : tr("identification"),
				type == QSmartCardData::Pin2Type ? tr("digital signing") : tr("verify identification"))
			);
	}
	setWindowTitle(ui->labelNameId->text());
	ui->labelPin->setText(tr("NEW %1 CODE").arg(QSmartCardData::typeString(type)));
	ui->labelRepeat->setText(tr("NEW %1 CODE AGAIN").arg(QSmartCardData::typeString(type)));
	ui->pin->setAccessibleName(ui->labelPin->text().toLower());
	ui->pin->setValidator(new QRegularExpressionValidator(regexpNewCode, ui->pin));
	ui->repeat->setAccessibleName(ui->labelRepeat->text().toLower());
	ui->repeat->setValidator(new QRegularExpressionValidator(regexpNewCode, ui->repeat));
	ui->puk->setAccessibleName(ui->labelPuk->text().toLower());
	ui->puk->setValidator(new QRegularExpressionValidator(regexpValidateCode, ui->puk));
	ui->change->setAccessibleName(ui->change->text().toLower());

	if(leftAttempts == 3)
		ui->errorPuk->clear();
	else if(mode == PinUnblock::PinChange)
		ui->errorPuk->setText(tr("Remaining attempts: %1").arg(leftAttempts));
	else
		ui->errorPuk->setText(tr("PUK remaining attempts: %1").arg(leftAttempts));

	for(int i = 1; i < 4; i++)
	{
		bool isHidden = true;
		if(auto *text = findChild<QLabel*>(QStringLiteral("line%1_text").arg(i)))
			text->setHidden(isHidden = text->text().isEmpty());
		if(auto *bullet = findChild<QLabel*>(QStringLiteral("line%1_bullet").arg(i)))
			bullet->setHidden(isHidden);
	}
	auto setError = [](QLineEdit *input, QLabel *error, const QString &msg) {
		input->setStyleSheet(msg.isEmpty() ? QString() :QStringLiteral("border-color: #c53e3e"));
		error->setFocusPolicy(msg.isEmpty() ? Qt::NoFocus : Qt::TabFocus);
		error->setText(msg);
	};
	connect(ui->cancel, &QPushButton::clicked, this, &PinUnblock::reject);
	connect(this, &PinUnblock::finished, this, &PinUnblock::close);
	connect(ui->pin, &QLineEdit::returnPressed, ui->change, &QPushButton::click);
	connect(ui->pin, &QLineEdit::textEdited, ui->errorPin, [this, setError] {
		setError(ui->pin, ui->errorPin, {});
	});
	connect(ui->repeat, &QLineEdit::returnPressed, ui->change, &QPushButton::click);
	connect(ui->repeat, &QLineEdit::textEdited, ui->errorRepeat, [this, setError] {
		setError(ui->repeat, ui->errorRepeat, {});
	});
	connect(ui->puk, &QLineEdit::returnPressed, ui->change, &QPushButton::click);
	connect(ui->puk, &QLineEdit::textEdited, ui->errorPuk, [this, setError] {
		setError(ui->puk, ui->errorPuk, {});
	});
	connect(ui->change, &QPushButton::clicked, this, [=,
			regexpNewCode = std::move(regexpNewCode),
			regexpValidateCode = std::move(regexpValidateCode)] {
		const static QString SEQUENCE_ASCENDING = QStringLiteral("1234567890123456789012");
		const static QString SEQUENCE_DESCENDING = QStringLiteral("0987654321098765432109");
		ui->puk->setStyleSheet({});
		ui->pin->setStyleSheet({});
		ui->repeat->setStyleSheet({});
		ui->errorPuk->clear();
		ui->errorPin->clear();
		ui->errorRepeat->clear();

		auto pinError = [](auto type) {
			return tr("%1 length has to be between %2 and 12")
				.arg(QSmartCardData::typeString(type)).arg(QSmartCardData::minPinLen(type));
		};

		// Verify checks
		if(!regexpValidateCode.match(ui->puk->text()).hasMatch())
			setError(ui->puk, ui->errorPuk, pinError(mode == PinUnblock::PinChange ? type : QSmartCardData::PukType));

		// PIN checks
		QString pin = ui->pin->text();
		if(!regexpNewCode.match(pin).hasMatch())
			return setError(ui->pin, ui->errorPin, pinError(type));
		if(SEQUENCE_ASCENDING.contains(pin))
			return setError(ui->pin, ui->errorPin,
				tr("New %1 code can't be increasing sequence").arg(QSmartCardData::typeString(type)));
		if(SEQUENCE_DESCENDING.contains(pin))
			return setError(ui->pin, ui->errorPin,
				tr("New %1 code can't be decreasing sequence").arg(QSmartCardData::typeString(type)));
		if(pin.count(pin[1]) == pin.length())
			return setError(ui->pin, ui->errorPin,
				tr("New %1 code can't be sequence of same numbers").arg(QSmartCardData::typeString(type)));
		if(personalCode.contains(pin))
			return setError(ui->pin, ui->errorPin,
				tr("New %1 code can't be part of your personal code").arg(QSmartCardData::typeString(type)));
		if(pin == birthDate.toString(QStringLiteral("yyyy")) ||
			pin == birthDate.toString(QStringLiteral("ddMM")) ||
			pin == birthDate.toString(QStringLiteral("MMdd")) ||
			pin == birthDate.toString(QStringLiteral("yyyyMMdd")) ||
			pin == birthDate.toString(QStringLiteral("ddMMyyyy")))
			return setError(ui->pin, ui->errorPin,
				tr("New %1 code can't be your date of birth").arg(QSmartCardData::typeString(type)));
		if(mode == PinUnblock::PinChange && pin == ui->puk->text())
			return setError(ui->pin, ui->errorPin,
				tr("Current %1 code and new %1 code must be different").arg(QSmartCardData::typeString(type)));

		// Repeat checks
		QString repeat = ui->repeat->text();
		if(!regexpNewCode.match(repeat).hasMatch())
			return setError(ui->repeat, ui->errorRepeat, pinError(type));
		if(pin != repeat)
			return setError(ui->repeat, ui->errorRepeat,
				tr("New %1 codes doesn't match").arg(QSmartCardData::typeString(type)));
		accept();
	});
	adjustSize();
	setFixedSize(size());
}

PinUnblock::~PinUnblock()
{
	delete ui;
}

QString PinUnblock::firstCodeText() const { return ui->puk->text(); }

QString PinUnblock::newCodeText() const { return ui->pin->text(); }

