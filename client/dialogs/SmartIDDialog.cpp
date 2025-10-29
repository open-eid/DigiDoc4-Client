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

#include "SmartIDDialog.h"
#include "ui_SmartIDDialog.h"

#include "IKValidator.h"
#include "Settings.h"
#include "effects/Overlay.h"

SmartIDDialog::SmartIDDialog(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::SmartIDDialog)
{
	static const QString &EE = Settings::SMARTID_COUNTRY_LIST.first();
	new Overlay(this);

	ui->setupUi(this);
	setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint);
#ifdef Q_OS_WIN
	ui->buttonLayout->setDirection(QBoxLayout::RightToLeft);
#endif

	ui->idCode->setAttribute(Qt::WA_MacShowFocusRect, false);
	ui->idCode->setFocus();
	ui->cbRemember->setAttribute(Qt::WA_MacShowFocusRect, false);
	ui->errorCode->hide();

	auto *ik = new NumberValidator(ui->idCode);
	ui->idCode->setValidator(Settings::SMARTID_COUNTRY == EE ? ik : nullptr);
	ui->idCode->setText(Settings::SMARTID_CODE);
	for(int i = 0, count = Settings::SMARTID_COUNTRY_LIST.size(); i < count; ++i)
		ui->idCountry->setItemData(i, Settings::SMARTID_COUNTRY_LIST[i]);
	ui->idCountry->setCurrentIndex(ui->idCountry->findData(Settings::SMARTID_COUNTRY));
	ui->cbRemember->setChecked(Settings::SMARTID_REMEMBER);
	auto saveSettings = [this]{
		bool checked = ui->cbRemember->isChecked();
		Settings::SMARTID_REMEMBER = checked;
		Settings::SMARTID_CODE = checked ? idCode() : QString();
		Settings::SMARTID_COUNTRY = checked ? country() : EE;
	};
	auto setError = [](LineEdit *input, QLabel *error, const QString &msg) {
		input->setLabel(msg.isEmpty() ? QString() : QStringLiteral("error"));
		error->setText(msg);
		error->setHidden(msg.isEmpty());
	};
	connect(ui->idCode, &QLineEdit::returnPressed, ui->sign, &QPushButton::click);
	connect(ui->idCode, &QLineEdit::textEdited, this, saveSettings);
	connect(ui->idCode, &QLineEdit::textEdited, ui->errorCode, [this, setError] {
		setError(ui->idCode, ui->errorCode, {});
	});
	connect(ui->idCountry, &QComboBox::currentTextChanged, this, [this, ik, saveSettings] {
		ui->idCode->setValidator(country() == EE ? ik : nullptr);
		saveSettings();
	});
	connect(ui->cbRemember, &QCheckBox::clicked, this, saveSettings);
	connect(ui->cancel, &QPushButton::clicked, this, &QDialog::reject);
	connect(ui->sign, &QPushButton::clicked, this, [this, setError] {
		if(ui->idCode->validator() && !IKValidator::isValid(idCode()))
			setError(ui->idCode, ui->errorCode, tr("Personal code is not valid"));
		else
		{
			setError(ui->idCode, ui->errorCode, {});
			accept();
		}
	});
}

SmartIDDialog::~SmartIDDialog()
{
	delete ui;
}

QString SmartIDDialog::country() const
{
	return ui->idCountry->currentData().toString();
}

QString SmartIDDialog::idCode() const
{
	return ui->idCode->text();
}
