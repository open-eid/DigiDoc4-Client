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
#include "common_enums.h"
#include "Styles.h"
#include "widgets/LabelButton.h"

using namespace ria::qdigidoc4;

CardWidget::CardWidget( QWidget *parent )
: CardWidget( QString(), parent ) { }

CardWidget::CardWidget( const QString &cardId, QWidget *parent )
: QWidget( parent )
, ui( new Ui::CardWidget )
, cardId( cardId )
{
	ui->setupUi( this );
	QFont font = Styles::font( Styles::Condensed, 16 );

	ui->cardName->setFont( Styles::font( Styles::Condensed, 20, QFont::DemiBold ) );
	ui->cardCode->setFont( font );
	ui->cardStatus->setFont( font );
	ui->cardPhoto->init( LabelButton::None, "", CardPhoto );
	ui->load->setFont(Styles::font(Styles::Condensed, 9));
	ui->load->hide();

	cardIcon.reset( new QSvgWidget( ":/images/icon_IDkaart_green.svg", this ) );
	cardIcon->resize( 17, 12 );
	cardIcon->move( 164, 42 );

	connect(ui->cardPhoto, &LabelButton::clicked, this, [this]() { emit photoClicked(ui->cardPhoto->pixmap()); });
	connect(ui->cardPhoto, &LabelButton::entered, this, [this]() { 
		if(!ui->cardPhoto->property("PICTURE").isValid())
			ui->load->show(); 
		});
	connect(ui->cardPhoto, &LabelButton::left, this, [this]() { ui->load->hide(); });
}

CardWidget::~CardWidget()
{
	delete ui;
}

void CardWidget::clearPicture()
{
	ui->cardPhoto->clear();
}

QString CardWidget::id() const
{
	return cardId;
}

bool CardWidget::event( QEvent *ev )
{
	if( ev->type() == QEvent::MouseButtonRelease )
	{
		emit selected( cardId );
	}
	return QWidget::event( ev );
}

void CardWidget::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange && cardInfo)
		update(cardInfo);

	QWidget::changeEvent(event);
}

bool CardWidget::isLoading() const
{
	return !cardInfo || cardInfo->loading;
}

void CardWidget::update(const QSharedPointer<const QCardInfo> &ci)
{
	cardInfo = ci;
	ui->cardName->setText(cardInfo->fullName);
	ui->cardCode->setText(cardInfo->id + "   |");
	ui->load->setText(tr("LOAD"));
	if(cardInfo->loading)
	{
		ui->cardStatus->setText(QString());
		cardIcon->load(QString(":/images/icon_IDkaart_disabled.svg"));
	}
	else
	{
		ui->cardStatus->setText(tr("%1 in reader").arg(tr(cardInfo->cardType.toLatin1())));
		cardIcon->load(QString(":/images/icon_IDkaart_green.svg"));
	}

	setAccessibleDescription(cardInfo->fullName);
}

void CardWidget::showPicture( const QPixmap &pix )
{
	ui->cardPhoto->setProperty( "PICTURE", pix );
	ui->cardPhoto->setPixmap( pix.scaled( 34, 44, Qt::IgnoreAspectRatio, Qt::SmoothTransformation ) );
}
