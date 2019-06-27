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

#include <common/SslCertificate.h>

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

	cardIcon.reset(new QSvgWidget(QStringLiteral(":/images/icon_IDkaart_green.svg"), this));
	cardIcon->setStyleSheet(QStringLiteral("background: none;"));
	cardIcon->resize( 17, 12 );
	cardIcon->move( 169, 42 );

	connect(ui->cardPhoto, &LabelButton::clicked, this, [this] {
		if(!seal)
			emit photoClicked(ui->cardPhoto->pixmap());
	});
	tr("e-Seal");
	tr("Digi-ID");
	tr("ID-card");
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
	if(ev->type() == QEvent::MouseButtonRelease)
	{
		emit selected( card );
		return true;
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

void CardWidget::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange && cardInfo)
		update(cardInfo, card);

	QWidget::changeEvent(event);
}

bool CardWidget::isLoading() const
{
	return !cardInfo || cardInfo->loading;
}

void CardWidget::update(const QSharedPointer<const QCardInfo> &ci, const QString &cardId)
{
	cardInfo = ci;
	card = cardId;
	ui->cardName->setText(cardInfo->fullName);
	ui->cardName->setAccessibleName(cardInfo->fullName.toLower());
	ui->cardCode->setText(cardInfo->id + "   |");
	ui->cardCode->setAccessibleName(cardInfo->id);
	ui->load->setText(tr("LOAD"));
	if(cardInfo->loading)
	{
		ui->cardStatus->clear();
		cardIcon->load(QStringLiteral(":/images/icon_IDkaart_disabled.svg"));
	}
	else
	{
		ui->cardStatus->setText(tr("%1 in reader").arg(tr(cardInfo->cardType)));
		cardIcon->load(QStringLiteral(":/images/icon_IDkaart_green.svg"));
	}

	if(ci->isEResident)
	{
		ui->horizontalSpacer->changeSize(1, 20, QSizePolicy::Fixed);
		ui->cardPhoto->hide();
		cardIcon->move(169 - 27, 42);
	}
	else
	{
		ui->horizontalSpacer->changeSize(10, 20, QSizePolicy::Fixed);
		ui->cardPhoto->show();
		cardIcon->move(169, 42);
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
