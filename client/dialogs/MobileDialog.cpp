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
#include "Styles.h"
#include "dialogs/SettingsDialog.h"
#include "effects/Overlay.h"

#include <QtCore/QSettings>

#define COUNTRY_CODE_EST QStringLiteral("372")
#define COUNTRY_CODE_LTU QStringLiteral("370")

MobileDialog::MobileDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::MobileDialog)
{
	new Overlay(this, parent);
	ui->setupUi(this);
	setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
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
	ui->idCode->setText(QSettings().value(QStringLiteral("MobileCode")).toString());
	ui->idCode->setAttribute(Qt::WA_MacShowFocusRect, false);
	ui->phoneNo->setValidator(new NumberValidator(ui->phoneNo));
	ui->phoneNo->setText(QSettings().value(QStringLiteral("MobileNumber"), COUNTRY_CODE_EST).toString());
	ui->phoneNo->setAttribute(Qt::WA_MacShowFocusRect, false);
	ui->cbRemember->setChecked(QSettings().value(QStringLiteral("MobileSettings"), true).toBool());
	ui->cbRemember->setAttribute(Qt::WA_MacShowFocusRect, false);
	auto saveSettings = [this] {
		bool checked = ui->cbRemember->isChecked();
		SettingsDialog::setValueEx(QStringLiteral("MobileSettings"), checked, true);
		SettingsDialog::setValueEx(QStringLiteral("MobileCode"), checked ? ui->idCode->text() : QString());
		SettingsDialog::setValueEx(QStringLiteral("MobileNumber"), checked ? ui->phoneNo->text() : QString());
	};
	connect(ui->idCode, &QLineEdit::returnPressed, ui->sign, &QPushButton::click);
	connect(ui->idCode, &QLineEdit::textEdited, this, saveSettings);
	connect(ui->phoneNo, &QLineEdit::returnPressed, ui->sign, &QPushButton::click);
	connect(ui->phoneNo, &QLineEdit::textEdited, this, saveSettings);
	connect(ui->cbRemember, &QCheckBox::clicked, this, saveSettings);
	connect(ui->sign, &QPushButton::clicked, this, [this] {
		static const QStringList countryCodes {COUNTRY_CODE_EST, COUNTRY_CODE_LTU};
		if(!IKValidator::isValid(idCode()))
		{
			ui->idCode->setStyleSheet(QStringLiteral("border-color: #c53e3e"));
			ui->errorCode->setText(tr("Personal code is not valid"));
		}
		else
		{
			ui->idCode->setStyleSheet({});
			ui->errorCode->clear();
		}
		if(phoneNo().size() < 8 || countryCodes.contains(phoneNo()))
		{
			ui->phoneNo->setStyleSheet(QStringLiteral("border-color: #c53e3e"));
			ui->errorPhone->setText(tr("Phone number is not entered"));
		}
		else if(!countryCodes.contains(phoneNo().left(3)))
		{
			ui->phoneNo->setStyleSheet(QStringLiteral("border-color: #c53e3e"));
			ui->errorPhone->setText(tr("Invalid country code"));
		}
		else
		{
			ui->phoneNo->setStyleSheet({});
			ui->errorPhone->clear();
		}
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
