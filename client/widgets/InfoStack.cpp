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

#include "InfoStack.h"
#include "ui_InfoStack.h"

#include "QSmartCard.h"
#include "SslCertificate.h"

#include <QStyle>

InfoStack::InfoStack( QWidget *parent )
	: QWidget(parent)
	, ui( new Ui::InfoStack )
{
	ui->setupUi( this );
	clearData();
}

InfoStack::~InfoStack()
{
	delete ui;
}

void InfoStack::clearData()
{
	certType = 0;
	expiry = {};
	ui->valueGivenNames->clear();
	ui->valueSurname->clear();
	ui->valuePersonalCode->clear();
	ui->valueCitizenship->clear();
	ui->valueExpiryDate->clear();
	ui->valueDocument->clear();
}

void InfoStack::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange)
	{
		ui->retranslateUi(this);
		update();
	}

	QWidget::changeEvent(event);
}

void InfoStack::update()
{
	ui->valueExpiryDate->setText(expiry.toString(QStringLiteral("dd.MM.yyyy")));
	if(certType & SslCertificate::DigiIDType)
	{
		ui->valueExpiryDate->setLabel({});
		ui->valueExpiryDate->setText(tr("You're using digital identity card"));
	} else if(expiry < QDateTime::currentDateTime())
		ui->valueExpiryDate->setLabel(QStringLiteral("error"));
	else
		ui->valueExpiryDate->setLabel(QStringLiteral("good"));

	ui->labelDocument->setHidden(certType & SslCertificate::TempelType);
	ui->valueCitizenship->setHidden(ui->valueCitizenship->text().isEmpty());
	ui->labelCitizenship->setHidden(ui->valueCitizenship->text().isEmpty());
	if(certType & SslCertificate::TempelType)
	{
		ui->labelGivenNames->setText(tr("Name"));
		ui->labelSurname->setText(tr("Organization"));
		ui->labelPersonalCode->setText(tr("Serial"));
		ui->labelCitizenship->setText(tr("Country"));
	}
	else
	{
		ui->labelGivenNames->setText(tr("Given names"));
		ui->labelSurname->setText(tr("Surname"));
		ui->labelPersonalCode->setText(tr("Personal code"));
		ui->labelCitizenship->setText(tr("Citizenship"));
	}
}

void InfoStack::update(const SslCertificate &cert)
{
	certType = cert.type();
	expiry = cert.expiryDate();
	ui->valueGivenNames->setText(cert.toString(QStringLiteral("CN")));
	ui->valueSurname->setText(cert.toString(QStringLiteral("O")));
	ui->valuePersonalCode->setText(cert.personalCode());
	ui->valueCitizenship->setText(cert.toString(QStringLiteral("C")));
	ui->valueDocument->clear();
	update();
}

void InfoStack::update(const QSmartCardData &t)
{
	if(t.isNull())
		return clearData();
	certType = t.authCert().type();
	expiry = t.data(QSmartCardData::Expiry).toDateTime();
	ui->valueGivenNames->setText(t.data(QSmartCardData::FirstName).toString());
	ui->valueSurname->setText(t.data(QSmartCardData::SurName).toString());
	ui->valuePersonalCode->setText(t.data(QSmartCardData::Id).toString());
	ui->valueCitizenship->setText(t.data(QSmartCardData::Citizen).toString());
	ui->valueDocument->setText(t.data(QSmartCardData::DocumentId).toString());
	update();
}
