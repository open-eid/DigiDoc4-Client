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

#include "Accordion.h"
#include "ui_Accordion.h"

Accordion::Accordion(QWidget *parent)
	: QWidget(parent)
	, ui(new Ui::Accordion)
{
	ui->setupUi( this );
	connect(ui->titleVerifyCert, &AccordionTitle::toggled, ui->contentVerifyCert, &QWidget::setVisible);
	connect(ui->authBox, &VerifyCert::changePinClicked, this, [this](QSmartCard::PinAction action) {
		emit changePinClicked(QSmartCardData::Pin1Type, action);
	});
	connect(ui->signBox, &VerifyCert::changePinClicked, this,[this](QSmartCard::PinAction action) {
		emit changePinClicked(QSmartCardData::Pin2Type, action);
	});
	connect(ui->pukBox, &VerifyCert::changePinClicked, this, [this](QSmartCard::PinAction action) {
		emit changePinClicked(QSmartCardData::PukType, action);
	});
	clear();
}

Accordion::~Accordion()
{
	delete ui;
}

void Accordion::clear()
{
	ui->authBox->clear();
	ui->signBox->clear();
	ui->pukBox->clear();
	ui->titleVerifyCert->setChecked(true);
}

void Accordion::updateInfo(const SslCertificate &c)
{
	clear();
	bool isSign = c.keyUsage().contains(SslCertificate::NonRepudiation);
	ui->authBox->setHidden(isSign);
	ui->signBox->setVisible(isSign);
	if(isSign)
		ui->signBox->update(QSmartCardData::Pin2Type, c);
	else
		ui->authBox->update(QSmartCardData::Pin1Type, c);

	ui->pukBox->hide();
}

void Accordion::updateInfo(const QSmartCardData &data)
{
	if(data.isNull())
		return clear();
	ui->authBox->setVisible(!data.authCert().isNull());
	if (!data.authCert().isNull())
		ui->authBox->update(QSmartCardData::Pin1Type, data);

	ui->signBox->setVisible(!data.signCert().isNull());
	if (!data.signCert().isNull())
		ui->signBox->update(QSmartCardData::Pin2Type, data);

	ui->pukBox->show();
	ui->pukBox->update(QSmartCardData::PukType, data);
}

void Accordion::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange)
		ui->retranslateUi(this);
	QWidget::changeEvent(event);
}
