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
	ui->cardCode->setFont( font );
	ui->cardStatus->setFont( font );
	ui->cardPhoto->init(LabelButton::None, QString(), CardPhoto);
	ui->cardPhoto->installEventFilter(this);
	ui->load->setFont(Styles::font(Styles::Condensed, 9));
	ui->load->hide();
	ui->cardIcon->load(QStringLiteral(":/images/icon_IDkaart_green.svg"));
	ui->contentLayout->setAlignment(ui->cardIcon, Qt::AlignBaseline);

	connect(ui->cardPhoto, &LabelButton::clicked, this, [this] {
		if(!seal)
			emit photoClicked(ui->cardPhoto->pixmap());
	});
}

CardWidget::CardWidget(QWidget *parent)
	: CardWidget(false, parent)
{}

CardWidget::~CardWidget()
{
	delete ui;
}

void CardWidget::clearPicture()
{
	clearSeal();
	ui->cardPhoto->clear();
}

void CardWidget::clearSeal()
{
	if(seal)
		seal->deleteLater();
	seal = nullptr;
	ui->cardPhoto->setCursor(QCursor(Qt::PointingHandCursor));
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
	if(o != ui->cardPhoto)
		return StyledWidget::eventFilter(o, e);
	switch(e->type())
	{
	case QEvent::HoverEnter:
		if(!ui->cardPhoto->pixmap() && !seal)
			ui->load->show();
		break;
	case QEvent::HoverLeave:
		ui->load->hide();
		break;
	default: break;
	}
	return StyledWidget::eventFilter(o, e);
}

void CardWidget::update(const TokenData &token, bool multiple)
{
	if(t != token)
		ui->cardPhoto->clear();
	t = token;
	SslCertificate c = t.cert();
	QString id = c.personalCode();
	if(!c.subjectInfo("GN").isEmpty() || !c.subjectInfo("SN").isEmpty())
		ui->cardName->setText(c.toString(QStringLiteral("GN SN")));
	else
		ui->cardName->setText(c.toString(QStringLiteral("CN")));
	ui->cardName->setAccessibleName(ui->cardName->text().toLower());
	ui->cardCode->setText(id + "   |");
	ui->cardCode->setAccessibleName(id);
	ui->load->setText(tr("LOAD"));

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
		typeString = ((isMultiple = multiple) ? tr("Selected is %1") : tr("%1 in reader")).arg(typeString);

	ui->cardStatus->setText(typeString);
	ui->cardIcon->load(QStringLiteral(":/images/icon_IDkaart_green.svg"));

	if(c.type() & SslCertificate::EResidentSubType)
	{
		ui->horizontalSpacer->changeSize(1, 20, QSizePolicy::Fixed);
		ui->cardPhoto->hide();
	}
	else
	{
		ui->horizontalSpacer->changeSize(10, 20, QSizePolicy::Fixed);
		ui->cardPhoto->show();
	}

	clearSeal();
	if(type & SslCertificate::TempelType)
	{
		ui->cardPhoto->clear();
		seal = new QSvgWidget(ui->cardPhoto);
		seal->load(QStringLiteral(":/images/icon_digitempel.svg"));
		seal->resize(32, 32);
		seal->move(1, 6);
		seal->show();
		seal->setStyleSheet(QStringLiteral("border: none;"));
		ui->cardPhoto->unsetCursor();
	}
}

void CardWidget::showPicture( const QPixmap &pix )
{
	clearSeal();
	ui->cardPhoto->setPixmap(pix.scaled(ui->cardPhoto->width(), ui->cardPhoto->height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
}
