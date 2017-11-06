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

#include "AddressItem.h"
#include "ui_AddressItem.h"

#include "Styles.h"
#include "dialogs/KeyDialog.h"
#include "effects/ButtonHoverFilter.h"

#include <common/SslCertificate.h>

#include <QSvgWidget>

using namespace ria::qdigidoc4;

AddressItem::AddressItem(ContainerState state, QWidget *parent, bool showIcon)
: Item(parent)
, ui(new Ui::AddressItem)
{
	ui->setupUi(this);
	if(showIcon)
	{
		auto icon = new QSvgWidget(":/images/icon_Krypto_small.svg", ui->icon);
		icon->resize(17, 19);
		icon->move(0, (this->height() - 19) / 2);
		icon->show();
	}
	ui->icon->setVisible(showIcon);
	ui->name->setFont( Styles::font( Styles::Regular, 14, QFont::DemiBold ) );
	ui->code->setFont( Styles::font( Styles::Regular, 14 ) );
	ui->idType->setFont( Styles::font( Styles::Regular, 11 ) );
	ui->remove->installEventFilter( new ButtonHoverFilter( ":/images/icon_remove.svg", ":/images/icon_remove_hover.svg", this ) );
	connect(ui->add, &QToolButton::clicked, [this](){ emit add(this);});
	connect(ui->remove, &QToolButton::clicked, [this](){ emit remove(this);});

	ui->add->setFont(Styles::font(Styles::Condensed, 12));
	ui->added->setFont(Styles::font(Styles::Condensed, 12));

	ui->add->hide();
	ui->added->hide();
	ui->added->setDisabled(true);
}

AddressItem::AddressItem(const CKey &k, ContainerState state, QWidget *parent)
: AddressItem(state, parent, true)
{
	key = k;
	QString name = !key.cert.subjectInfo("GN").isEmpty() && !key.cert.subjectInfo("SN").isEmpty() ?
			key.cert.subjectInfo("GN").value(0) + " " + key.cert.subjectInfo("SN").value(0) :
			key.cert.subjectInfo("CN").value(0);

	QString type;
	auto certType = SslCertificate(key.cert).type();
	if(certType & SslCertificate::DigiIDType)
		type = "Digi-ID";
	else if(certType & SslCertificate::EstEidType)
		type = "ID-card";
	else if(SslCertificate::MobileIDType)
		type = "Mobile-ID";
	else
		type = "Unknown ID";

	update(name, key.cert.subjectInfo("serialNumber").value(0), type, AddressItem::Remove);
}

AddressItem::~AddressItem()
{
	delete ui;
}

void AddressItem::idChanged(const QString& cardCode, const QString& mobileCode)
{
	if(code == cardCode)
		ui->code->setText(code + tr(" (Yourself)"));
	else
		ui->code->setText(code);
}

void AddressItem::mouseReleaseEvent(QMouseEvent *event)
{
	KeyDialog dlg(key, this);
	dlg.exec();
}

void AddressItem::update(const QString& name, const QString& cardCode, const QString& type, ShowToolButton show)
{
	ui->name->setText( name );
	ui->code->setText( cardCode );
	ui->idType->setText( tr(qPrintable(type)));
	typeText = type;
	code = cardCode;

	showButton(show);
}

void AddressItem::showButton(ShowToolButton show)
{
	switch (show) {
	case Remove:
		ui->remove->show();
		ui->add->hide();
		ui->added->hide();
		break;
	case Add:
		ui->remove->hide();
		ui->add->show();
		ui->added->hide();
		break;
	case Added:
		ui->remove->hide();
		ui->add->hide();
		ui->added->show();
		break;
	default:
		ui->remove->hide();
		ui->add->hide();
		ui->added->hide();
		break;
	}
}

const CKey& AddressItem::getKey() const
{
	return key;
}

void AddressItem::stateChange(ContainerState state)
{
	if( state == UnencryptedContainer )
	{
		ui->remove->show();
	}
	else
	{
		ui->remove->hide();
	}
}

void AddressItem::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange)
	{
		ui->retranslateUi(this);

		idChanged(code, "");
		ui->idType->setText(tr(qPrintable(typeText)));
	}

	QWidget::changeEvent(event);
}
