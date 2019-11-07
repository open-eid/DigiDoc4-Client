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

#include "QCardInfo.h"
#include "common_enums.h"
#include "Styles.h"

#include <QSvgWidget>

using namespace ria::qdigidoc4;

CardWidget::CardWidget( QWidget *parent )
	: CardWidget( QString(), parent ) { }

CardWidget::CardWidget(QString id, QWidget *parent)
	: StyledWidget(parent)
	, ui(new Ui::CardWidget)
	, card(std::move(id))
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

QString CardWidget::id() const
{
	return card;
}

bool CardWidget::event( QEvent *ev )
{
	switch(ev->type())
	{
	case QEvent::MouseButtonRelease:
		emit selected( card );
		return true;
	case QEvent::LanguageChange:
		ui->retranslateUi(this);
		if(cardInfo)
			update(cardInfo, card);
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

bool CardWidget::isLoading() const
{
	return !cardInfo || cardInfo->loading;
}

void CardWidget::update(const QSharedPointer<const QCardInfo> &ci, const QString &cardId)
{
	cardInfo = ci;
	card = cardId;
	if(!ci->c.subjectInfo("GN").isEmpty() || !ci->c.subjectInfo("SN").isEmpty())
		ui->cardName->setText(ci->c.toString(QStringLiteral("GN SN")));
	else
		ui->cardName->setText(ci->c.toString(QStringLiteral("CN")));
	ui->cardName->setAccessibleName(ui->cardName->text().toLower());
	ui->cardCode->setText(cardInfo->id + "   |");
	ui->cardCode->setAccessibleName(cardInfo->id);
	ui->load->setText(tr("LOAD"));
	if(cardInfo->loading)
	{
		ui->cardStatus->clear();
		ui->cardIcon->load(QStringLiteral(":/images/icon_IDkaart_disabled.svg"));
	}
	else
	{
		QString type = tr("ID-card");
		if(ci->type & SslCertificate::TempelType &&
			ci->c.enhancedKeyUsage().contains(SslCertificate::ClientAuth))
			type = tr("Authentication certificate");
		else if(ci->type & SslCertificate::TempelType && (
			ci->c.keyUsage().contains(SslCertificate::KeyEncipherment) ||
			ci->c.keyUsage().contains(SslCertificate::KeyAgreement)))
			type = tr("Certificate for Encryption");
		else if(ci->type & SslCertificate::TempelType)
			type = tr("e-Seal");
		else if(ci->type & SslCertificate::DigiIDType)
			type = tr("Digi-ID");
		ui->cardStatus->setText(tr("%1 in reader").arg(type));
		ui->cardIcon->load(QStringLiteral(":/images/icon_IDkaart_green.svg"));
	}

	if(ci->c.subjectInfo("O").contains(QStringLiteral("E-RESIDENT")))
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
	if(cardInfo->type & SslCertificate::TempelType)
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
