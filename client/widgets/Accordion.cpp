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

#include "QCardInfo.h"

Accordion::Accordion(QWidget *parent)
	: StyledWidget(parent)
	, ui(new Ui::Accordion)
{
	ui->setupUi( this );
}

Accordion::~Accordion()
{
	delete ui;
}

void Accordion::init()
{
	// Initialize accordion.
	openSection = ui->titleVerifyCert;

	connect(ui->contentOtherData, &OtherData::checkEMailClicked, this, &Accordion::checkEMail);
	connect(ui->contentOtherData, &OtherData::activateEMailClicked, this, &Accordion::activateEMail);

	ui->titleVerifyCert->init(true, tr("PIN/PUK CODES AND CERTIFICATES"), tr("PIN/PUK codes and certificates", "accessible"), ui->contentVerifyCert);
	ui->titleVerifyCert->setClosable(true);
	ui->titleOtherData->init(false, tr("REDIRECTION OF EESTI.EE E-MAIL"), tr("Redirection of eesti.ee e-mail", "accessible"), ui->contentOtherData);
	ui->titleOtherData->setClosable(true);

	connect(ui->titleVerifyCert, &AccordionTitle::opened, this, &Accordion::closeOtherSection);
	connect(ui->titleVerifyCert, &AccordionTitle::closed, this, [this] {open(ui->titleOtherData);});
	connect(ui->titleOtherData, &AccordionTitle::opened, this, &Accordion::closeOtherSection);
	connect(ui->titleOtherData, &AccordionTitle::closed, this, [this] {open(ui->titleVerifyCert);});

	connect(ui->authBox, &VerifyCert::changePinClicked, this, &Accordion::changePin1Clicked);
	connect(ui->signBox, &VerifyCert::changePinClicked, this, &Accordion::changePin2Clicked);
	connect(ui->pukBox, &VerifyCert::changePinClicked, this, &Accordion::changePukClicked);

	// Initialize PIN/PUK content widgets.
	ui->signBox->addBorders();

	clear();
}

void Accordion::clear()
{
	ui->authBox->clear();
	ui->signBox->clear();
	ui->pukBox->clear();
	// Set to default Certificate Info page
	ui->contentOtherData->update(false, QByteArray());
	closeOtherSection(ui->titleVerifyCert);
	ui->titleVerifyCert->setSectionOpen(true);
}

void Accordion::closeOtherSection(AccordionTitle* opened)
{
	openSection->setSectionOpen(false);
	openSection = opened;
}

bool Accordion::updateOtherData(const QByteArray &data)
{
	return ui->contentOtherData->update(false, data);
}

QString Accordion::getEmail()
{
	return ui->contentOtherData->getEmail();
}

void Accordion::open(AccordionTitle* opened)
{
	if (opened->isVisible ())
		openSection = opened;
	openSection->setSectionOpen(true);
}

void Accordion::setFocusToEmail()
{
	ui->contentOtherData->setFocusToEmail();
}

void Accordion::updateInfo(const QCardInfo &info)
{
	bool isSign = info.c.keyUsage().contains(SslCertificate::NonRepudiation);
	ui->authBox->setHidden(isSign);
	ui->signBox->setVisible(isSign);
	if(isSign)
		ui->signBox->update(QSmartCardData::Pin2Type, info.c);
	else
		ui->authBox->update(QSmartCardData::Pin1Type, info.c);

	ui->pukBox->hide();
	ui->titleOtherData->hide();
}

void Accordion::updateInfo( const QSmartCard *smartCard )
{
	QSmartCardData t = smartCard->data();

	ui->authBox->setVisible( !t.authCert().isNull() );
	if ( !t.authCert().isNull() )
		ui->authBox->update( QSmartCardData::Pin1Type, smartCard );

	ui->signBox->setVisible( !t.signCert().isNull() );
	if ( !t.signCert().isNull() )
		ui->signBox->update( QSmartCardData::Pin2Type, smartCard );

	ui->pukBox->show();
	ui->pukBox->update( QSmartCardData::PukType, smartCard );

	ui->titleOtherData->setHidden(t.version() == QSmartCardData::VER_USABLEUPDATER ||
		t.authCert().subjectInfo("O").contains(QStringLiteral("E-RESIDENT")));
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
