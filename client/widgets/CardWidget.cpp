// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "CardWidget.h"
#include "ui_CardWidget.h"

#include "SslCertificate.h"

using namespace ria::qdigidoc4;

CardWidget::CardWidget(bool popup, QWidget *parent)
	: StyledWidget(parent)
	, ui(new Ui::CardWidget)
	, isPopup(popup)
{
	ui->setupUi( this );
	ui->cardIcon->load(QStringLiteral(":/images/icon_IDkaart_green.svg"));
	ui->CardWidgetLayout->setAlignment(ui->cardIcon, Qt::AlignBaseline);
}

CardWidget::CardWidget(QWidget *parent)
	: CardWidget(false, parent)
{}

CardWidget::~CardWidget()
{
	delete ui;
}

TokenData CardWidget::token() const
{
	return t;
}

bool CardWidget::event( QEvent *ev )
{
	switch(ev->type())
	{
	case QEvent::MouseButtonRelease:
		emit selected(t);
		return true;
	case QEvent::LanguageChange:
		ui->retranslateUi(this);
		update(t, isMultiple);
		break;
	default: break;
	}
	return QWidget::event( ev );
}

void CardWidget::update(const TokenData &token, bool multiple)
{
	t = token;
	SslCertificate c = t.cert();
	QString id = c.personalCode();
	if(!c.subjectInfo("GN").isEmpty() || !c.subjectInfo("SN").isEmpty())
		ui->cardName->setText(c.toString(QStringLiteral("GN SN")).toHtmlEscaped());
	else
		ui->cardName->setText(c.toString(QStringLiteral("CN")).toHtmlEscaped());
	ui->cardName->setAccessibleName(ui->cardName->text().toLower());
	ui->cardCode->setText(id + "   |");
	ui->cardCode->setAccessibleName(id);

	int type = c.type();
	QString typeString = tr("ID-card");
	if(type & SslCertificate::TempelType &&
		c.enhancedKeyUsage().contains(SslCertificate::ClientAuth))
		typeString = tr("Authentication certificate");
	else if(type & SslCertificate::TempelType && (
		c.keyUsage().contains(SslCertificate::KeyEncipherment) ||
		c.keyUsage().contains(SslCertificate::KeyAgreement)))
		typeString = tr("Certificate for Encryption");
	else if(type & SslCertificate::TempelType)
		typeString = tr("e-Seal");
	else if(type & SslCertificate::DigiIDType)
		typeString = tr("Digi-ID");
	if(!isPopup)
		typeString = ((isMultiple = multiple) ? tr("%1 is selected") : tr("%1 in reader")).arg(typeString);

	ui->cardStatus->setText(typeString);
}
