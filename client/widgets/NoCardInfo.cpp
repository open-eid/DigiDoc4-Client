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

#include "NoCardInfo.h"
#include "ui_NoCardInfo.h"
#include "Styles.h"

NoCardInfo::NoCardInfo( QWidget *parent )
	: QWidget(parent)
	, ui(new Ui::NoCardInfo)
{
	ui->setupUi( this );
	ui->cardStatus->setFont( Styles::font( Styles::Condensed, 16 ) );
	ui->cardIcon->load(QStringLiteral(":/images/icon_IDkaart_red.svg"));
}

NoCardInfo::~NoCardInfo()
{
	delete ui;
}

void NoCardInfo::update(Status s)
{
	status = s;
	switch(status)
	{
	case NoPCSC:
		ui->cardStatus->setText(tr("The PCSC service, required for using the ID-card, is not working. Check your computer settings."));
		break;
	case NoReader:
		ui->cardStatus->setText(tr("No readers found"));
		break;
	case Loading:
		ui->cardStatus->setText(tr("Loading data"));
		break;
	default:
		ui->cardStatus->setText(tr("No card in reader"));
		break;
	}
	setAccessibleDescription(ui->cardStatus->text());
}

void NoCardInfo::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange)
		update(status);

	QWidget::changeEvent(event);
}
