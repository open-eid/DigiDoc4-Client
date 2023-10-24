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

#include "MobileDialog.h"
#include "ui_MobileDialog.h"

#include "IKValidator.h"
#include "Settings.h"
#include "Styles.h"
#include "effects/Overlay.h"

MobileDialog::MobileDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::MobileDialog)
{
	static const QStringList countryCodes {QStringLiteral("372"), QStringLiteral("370")};
	new Overlay(this);
	ui->setupUi(this);
	setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint);
	setFixedSize(size());
#ifdef Q_OS_WIN
	ui->buttonLayout->setDirection(QBoxLayout::RightToLeft);
#endif

	QFont condensed = Styles::font(Styles::Condensed, 14);
	QFont regularFont = Styles::font(Styles::Regular, 14);
	ui->labelNameId->setFont(Styles::font(Styles::Regular, 16, QFont::DemiBold));
	ui->labelPhone->setFont(regularFont);
	ui->labelCode->setFont(regularFont);
	ui->errorCode->setFont(regularFont);
	ui->errorPhone->setFont(regularFont);
	ui->phoneNo->setFont(regularFont);
	ui->idCode->setFont(regularFont);
	ui->cbRemember->setFont(regularFont);
	ui->sign->setFont(condensed);
	ui->cancel->setFont(condensed);

	// Mobile
	ui->idCode->setValidator(new NumberValidator(ui->idCode));
	ui->idCode->setText(Settings::MOBILEID_CODE);
	ui->idCode->setAttribute(Qt::WA_MacShowFocusRect, false);
	ui->phoneNo->setValidator(new NumberValidator(ui->phoneNo));
	ui->phoneNo->setText(Settings::MOBILEID_NUMBER.value(countryCodes[0]));
	ui->phoneNo->setAttribute(Qt::WA_MacShowFocusRect, false);
	ui->phoneNo->setFocus();
	ui->cbRemember->setChecked(Settings::MOBILEID_REMEMBER);
	ui->cbRemember->setAttribute(Qt::WA_MacShowFocusRect, false);
	auto saveSettings = [this] {
		bool checked = ui->cbRemember->isChecked();
		Settings::MOBILEID_REMEMBER = checked;
		Settings::MOBILEID_CODE = checked ? ui->idCode->text() : QString();
		Settings::MOBILEID_NUMBER = checked ? ui->phoneNo->text() : QString();
	};
	auto setError = [](QLineEdit *input, QLabel *error, const QString &msg) {
		input->setStyleSheet(msg.isEmpty() ? QString() :QStringLiteral("border-color: #c53e3e"));
		error->setFocusPolicy(msg.isEmpty() ? Qt::NoFocus : Qt::TabFocus);
		error->setText(msg);
	};
	connect(ui->idCode, &QLineEdit::returnPressed, ui->sign, &QPushButton::click);
	connect(ui->idCode, &QLineEdit::textEdited, this, saveSettings);
	connect(ui->idCode, &QLineEdit::textEdited, ui->errorCode, [this, setError] {
		setError(ui->idCode, ui->errorCode, {});
	});
	connect(ui->phoneNo, &QLineEdit::returnPressed, ui->sign, &QPushButton::click);
	connect(ui->phoneNo, &QLineEdit::textEdited, this, saveSettings);
	connect(ui->phoneNo, &QLineEdit::textEdited, ui->errorPhone, [this, setError] {
		setError(ui->phoneNo, ui->errorPhone, {});
	});
	connect(ui->cbRemember, &QCheckBox::clicked, this, saveSettings);
	connect(ui->sign, &QPushButton::clicked, this, [this, setError] {
		if(!IKValidator::isValid(idCode()))
			setError(ui->idCode, ui->errorCode, tr("Personal code is not valid"));
		else
			setError(ui->idCode, ui->errorCode, {});
		if(phoneNo().size() < 8 || countryCodes.contains(phoneNo()))
			setError(ui->phoneNo, ui->errorPhone, tr("Phone number is not entered"));
		else if(!countryCodes.contains(phoneNo().left(3)))
			setError(ui->phoneNo, ui->errorPhone, tr("Invalid country code"));
		else
			setError(ui->phoneNo, ui->errorPhone, {});
		if(ui->errorCode->text().isEmpty() && ui->errorPhone->text().isEmpty())
			accept();
	});
	connect(ui->cancel, &QPushButton::clicked, this, &MobileDialog::reject);
	connect(this, &MobileDialog::finished, this, &MobileDialog::close);
}

MobileDialog::~MobileDialog()
{
	delete ui;
}

QString MobileDialog::idCode()
{
	return ui->idCode->text();
}

QString MobileDialog::phoneNo()
{
	return ui->phoneNo->text();
}
