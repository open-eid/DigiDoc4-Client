#include "SmartIDDialog.h"
#include "ui_SmartIDDialog.h"

#include "Styles.h"
#include "effects/Overlay.h"

#include <QtCore/QSettings>

#include <common/Common.h>
#include <common/IKValidator.h>

SmartIDDialog::SmartIDDialog(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::SmartIDDialog)
{
	Overlay *overlay = new Overlay(parent->topLevelWidget());
	overlay->show();
	connect(this, &SmartIDDialog::destroyed, overlay, &Overlay::deleteLater);

	ui->setupUi(this);
	setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
	setWindowModality( Qt::ApplicationModal);
	setFixedSize(size());
	QFont condensed12 = Styles::font(Styles::Condensed, 12);
	QFont condensed14 = Styles::font(Styles::Condensed, 14);
	QFont header = Styles::font(Styles::Regular, 14);
	QFont regularFont = Styles::font(Styles::Regular, 14);
	header.setWeight(QFont::DemiBold);
	ui->cbRemember->setFont(Styles::font(Styles::Regular, 14));
	ui->labelNameId->setFont(header);
	ui->labelCode->setFont(condensed12);
	ui->labelCountry->setFont(condensed12);
	ui->idCode->setFont(regularFont);
	ui->idCountry->setFont(regularFont);
	ui->sign->setFont(condensed14);
	ui->cancel->setFont(condensed14);

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
		Common::setValueEx(QStringLiteral("SmartIDSettings"), checked, true);
		Common::setValueEx(QStringLiteral("SmartID"), checked ? idCode() : QString(), QString());
		Common::setValueEx(QStringLiteral("SmartIDCountry"), checked ? country() : EE, EE);
		ui->sign->setToolTip(QString());
		if(!idCode().isEmpty() && country() == EE && !IKValidator::isValid(idCode()))
			ui->sign->setToolTip(tr("Personal code is not valid"));
		ui->sign->setEnabled(!idCode().isEmpty() && ui->sign->toolTip().isEmpty());
	};
	connect(ui->idCode, &QLineEdit::textEdited, this, enableSign);
	connect(ui->idCountry, &QComboBox::currentTextChanged, this, enableSign);
	connect(ui->cbRemember, &QCheckBox::clicked, this, enableSign);
	enableSign();
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
