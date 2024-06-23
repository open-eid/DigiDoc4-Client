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

#include "CardWidget.h"
#include "ui_CardWidget.h"

#include "Styles.h"
#include "SslCertificate.h"
#include "common_enums.h"

using namespace ria::qdigidoc4;

CardWidget::CardWidget(bool popup, QWidget *parent)
	: StyledWidget(parent)
	, ui(new Ui::CardWidget)
	, isPopup(popup)
{
	ui->setupUi( this );
	QFont font = Styles::font( Styles::Condensed, 16 );

	ui->cardName->setFont( Styles::font( Styles::Condensed, 20, QFont::DemiBold ) );
	ui->cardName->installEventFilter(this);
	ui->cardCode->setFont( font );
	ui->cardCode->installEventFilter(this);
	ui->cardStatus->setFont( font );
	ui->cardStatus->installEventFilter(this);
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

bool CardWidget::eventFilter(QObject *o, QEvent *e)
{
	if(e->type() == QEvent::MouseButtonRelease &&
		(o == ui->cardName || o == ui->cardStatus || o == ui->cardCode))
	{
		emit selected(t);
		return true;
	}
	return StyledWidget::eventFilter(o, e);
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
	ui->cardIcon->load(QStringLiteral(":/images/icon_IDkaart_green.svg"));
}
