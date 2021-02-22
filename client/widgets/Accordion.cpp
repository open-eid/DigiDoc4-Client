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
	: StyledWidget(parent)
	, ui(new Ui::Accordion)
{
	ui->setupUi( this );
	connect(ui->contentOtherData, &OtherData::checkEMailClicked, this, &Accordion::checkEMail);
	connect(ui->contentOtherData, &OtherData::activateEMailClicked, this, &Accordion::activateEMail);

	ui->titleVerifyCert->init(true, tr("PIN/PUK CODES AND CERTIFICATES"), tr("PIN/PUK codes and certificates", "accessible"), ui->contentVerifyCert);
	ui->titleOtherData->init(false, tr("REDIRECTION OF EESTI.EE E-MAIL"), tr("Redirection of eesti.ee e-mail", "accessible"), ui->contentOtherData);

	connect(ui->titleVerifyCert, &AccordionTitle::opened, ui->titleOtherData, &AccordionTitle::openSection);
	connect(ui->titleVerifyCert, &AccordionTitle::closed, this, [this] { ui->titleOtherData->setSectionOpen(); });
	connect(ui->titleOtherData, &AccordionTitle::opened, ui->titleVerifyCert, &AccordionTitle::openSection);
	connect(ui->titleOtherData, &AccordionTitle::closed, this, [this] { ui->titleVerifyCert->setSectionOpen(); });

	connect(ui->authBox, &VerifyCert::changePinClicked, this, &Accordion::changePin1Clicked);
	connect(ui->signBox, &VerifyCert::changePinClicked, this, &Accordion::changePin2Clicked);
	connect(ui->pukBox, &VerifyCert::changePinClicked, this, &Accordion::changePukClicked);

	// Initialize PIN/PUK content widgets.
	ui->signBox->addBorders();

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
	ui->titleVerifyCert->setSectionOpen();
	updateOtherData({});
	ui->titleOtherData->hide();
}

bool Accordion::updateOtherData(const QByteArray &data)
{
	return ui->contentOtherData->update(false, data);
}

QString Accordion::getEmail()
{
	return ui->contentOtherData->getEmail();
}

void Accordion::setFocusToEmail()
{
	ui->contentOtherData->setFocusToEmail();
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
	ui->titleOtherData->hide();
}

void Accordion::updateInfo(const QSmartCardData &data)
{
	ui->authBox->setVisible(!data.authCert().isNull());
	if (!data.authCert().isNull())
		ui->authBox->update(QSmartCardData::Pin1Type, data);

	ui->signBox->setVisible(!data.signCert().isNull());
	if (!data.signCert().isNull())
		ui->signBox->update(QSmartCardData::Pin2Type, data);

	ui->pukBox->show();
	ui->pukBox->update(QSmartCardData::PukType, data);

	ui->titleOtherData->setHidden(data.version() == QSmartCardData::VER_USABLEUPDATER);
	if(ui->contentOtherData->property("card").toString() != data.card())
		ui->contentOtherData->update(false, {});
	ui->contentOtherData->setProperty("card", data.card());
}

void Accordion::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange)
	{
		ui->retranslateUi(this);
		ui->titleVerifyCert->setText(tr("PIN/PUK CODES AND CERTIFICATES"), tr("PIN/PUK codes and certificates", "accessible"));
		ui->titleOtherData->setText(tr("REDIRECTION OF EESTI.EE E-MAIL"), tr("Redirection of eesti.ee e-mail", "accessible"));
	}
	QWidget::changeEvent(event);
}
