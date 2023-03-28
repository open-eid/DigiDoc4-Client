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
#include "Styles.h"
#include "effects/Overlay.h"

SmartIDDialog::SmartIDDialog(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::SmartIDDialog)
{
	static const QString &EE = Settings::SMARTID_COUNTRY_LIST[0];
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
	ui->labelCode->setFont(regularFont);
	ui->labelCountry->setFont(regularFont);
	ui->errorCode->setFont(regularFont);
	ui->errorCountry->setFont(regularFont);
	ui->idCode->setFont(regularFont);
	ui->idCode->setAttribute(Qt::WA_MacShowFocusRect, false);
	ui->idCountry->setFont(regularFont);
	ui->cbRemember->setFont(regularFont);
	ui->cbRemember->setAttribute(Qt::WA_MacShowFocusRect, false);
	ui->sign->setFont(condensed);
	ui->cancel->setFont(condensed);

	QValidator *ik = new NumberValidator(ui->idCode);
	ui->idCode->setValidator(ik);
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
	connect(ui->idCode, &QLineEdit::returnPressed, ui->sign, &QPushButton::click);
	connect(ui->idCode, &QLineEdit::textEdited, this, saveSettings);
	connect(ui->idCode, &QLineEdit::textEdited, ui->errorCode, [this] {
		ui->errorCode->clear();
		ui->idCode->setStyleSheet({});
	});
	connect(ui->idCountry, &QComboBox::currentTextChanged, this, saveSettings);
	connect(ui->cbRemember, &QCheckBox::clicked, this, saveSettings);
	connect(ui->cancel, &QPushButton::clicked, this, &QDialog::reject);
	connect(ui->sign, &QPushButton::clicked, this, [this,ik] {
		ui->idCode->setValidator(country() == EE ? ik : nullptr);
		if(ui->idCode->validator() && !IKValidator::isValid(idCode()))
		{
			ui->idCode->setStyleSheet(QStringLiteral("border-color: #c53e3e"));
			ui->errorCode->setText(tr("Personal code is not valid"));
		}
		else
		{
			ui->idCode->setStyleSheet({});
			ui->errorCode->clear();
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
