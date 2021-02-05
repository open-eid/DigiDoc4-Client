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

#include "Styles.h"
#include "dialogs/SettingsDialog.h"
#include "effects/Overlay.h"

#include <QtCore/QSettings>

#include <common/IKValidator.h>

SmartIDDialog::SmartIDDialog(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::SmartIDDialog)
{
	new Overlay(this, parent->topLevelWidget());

	ui->setupUi(this);
	setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
	setWindowModality( Qt::ApplicationModal);
	setFixedSize(size());
	QFont condensed = Styles::font(Styles::Condensed, 14);
	QFont header = Styles::font(Styles::Regular, 16, QFont::DemiBold);
	QFont regularFont = Styles::font(Styles::Regular, 14);
	ui->labelNameId->setFont(header);
	ui->labelCode->setFont(regularFont);
	ui->labelCountry->setFont(regularFont);
	ui->idCode->setFont(regularFont);
	ui->idCode->setAttribute(Qt::WA_MacShowFocusRect, 0);
	ui->idCountry->setFont(regularFont);
	ui->cbRemember->setFont(regularFont);
	ui->cbRemember->setAttribute(Qt::WA_MacShowFocusRect, 0);
	ui->sign->setFont(condensed);
	ui->cancel->setFont(condensed);

	QSettings s;
	QValidator *ik = new IKValidator(ui->idCode);
	ui->idCode->setValidator(ik);
	ui->idCode->setText(s.value(QStringLiteral("SmartID")).toString());
	ui->idCountry->setItemData(0, "EE");
	ui->idCountry->setItemData(1, "LT");
	ui->idCountry->setItemData(2, "LV");
	ui->idCountry->setCurrentIndex(ui->idCountry->findData(s.value(QStringLiteral("SmartIDCountry"), QStringLiteral("EE")).toString()));
	ui->cbRemember->setChecked(s.value(QStringLiteral("SmartIDSettings"), true).toBool());
	connect(ui->sign, &QPushButton::clicked, this, &QDialog::accept);
	connect(ui->cancel, &QPushButton::clicked, this, &QDialog::reject);

	auto enableSign = [=] {
		const QString EE = QStringLiteral("EE");
		ui->idCode->setValidator(country() == EE ? ik : nullptr);
		bool checked = ui->cbRemember->isChecked();
		SettingsDialog::setValueEx(QStringLiteral("SmartIDSettings"), checked, true);
		SettingsDialog::setValueEx(QStringLiteral("SmartID"), checked ? idCode() : QString());
		SettingsDialog::setValueEx(QStringLiteral("SmartIDCountry"), checked ? country() : EE, EE);
		ui->sign->setToolTip(QString());
		if(!idCode().isEmpty() && country() == EE && !IKValidator::isValid(idCode()))
			ui->sign->setToolTip(tr("Personal code is not valid"));
		ui->sign->setEnabled(!idCode().isEmpty() && ui->sign->toolTip().isEmpty());
	};
	connect(ui->idCode, &QLineEdit::textEdited, this, enableSign);
	connect(ui->idCountry, &QComboBox::currentTextChanged, this, enableSign);
	connect(ui->cbRemember, &QCheckBox::clicked, this, enableSign);
	enableSign();
	ui->sign->setFocus();
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
