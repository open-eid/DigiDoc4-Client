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

#include <common/DateTime.h>
#include <common/SslCertificate.h>

#include <QResizeEvent>
#include <QSvgWidget>
#include <QTextStream>

using namespace ria::qdigidoc4;

#define ADD_WIDTH 32
#define ADDED_WIDTH 51
#define ADDRESS_ITEM "%1 <span style=\"font-weight:normal;\">%2</span>"
#define ICON_WIDTH 19
#define ITEM_HEIGHT 44
#define LINE_HEIGHT 16

AddressItem::AddressItem(const CKey &k, QWidget *parent, bool showIcon)
: Item(parent)
, ui(new Ui::AddressItem)
, enlarged(false)
, key(k)
, nameWidth(0)
, reservedWidth(false)
, yourself(false)

{
	ui->setupUi(this);
	if(showIcon)
	{
		auto icon = new QSvgWidget(":/images/icon_Krypto_small.svg", ui->icon);
		icon->resize(17, 19);
		icon->move(0, (this->height() - 19) / 2);
		icon->show();
	}
	QFont nameFont = Styles::font(Styles::Regular, 14, QFont::DemiBold);
	ui->icon->setVisible(showIcon);
	ui->name->setFont(nameFont);
	nameMetrics.reset(new QFontMetrics(nameFont));
	ui->idType->setFont( Styles::font( Styles::Regular, 11 ) );

	ui->remove->setIcons("/images/icon_remove.svg", "/images/icon_remove_hover.svg", "/images/icon_remove_pressed.svg", 1, 1, 17, 17);
	ui->remove->init(LabelButton::White, "", 0);
	connect(ui->add, &QToolButton::clicked, [this](){ emit add(this);});
	connect(ui->remove, &LabelButton::clicked, [this](){ emit remove(this);});

	ui->add->setFont(Styles::font(Styles::Condensed, 12));
	ui->added->setFont(Styles::font(Styles::Condensed, 12));

	ui->add->hide();
	ui->added->hide();
	ui->added->setDisabled(true);

	QString name = !key.cert.subjectInfo("GN").isEmpty() && !key.cert.subjectInfo("SN").isEmpty() ?
			key.cert.subjectInfo("GN").value(0) + " " + key.cert.subjectInfo("SN").value(0) :
			key.cert.subjectInfo("CN").value(0);

	QString type, strDate;

	auto certType = SslCertificate(key.cert).type();
	if(certType & SslCertificate::DigiIDType)
		type = "Digi-ID";
	else if(certType & SslCertificate::EstEidType)
		type = "ID-card";
	else if(certType & SslCertificate::TempelType)
		type = "e-Seal";
	else if(certType & SslCertificate::MobileIDType)
		type = "Mobile-ID";

	if(!showIcon)
	{
		DateTime date(key.cert.expiryDate().toLocalTime());
		if(!date.isNull())
		{
			strDate = date.formatDate("dd. MMMM yyyy");
		}
	}

	update(name, key.cert.subjectInfo("serialNumber").value(0), type, strDate, AddressItem::Remove);
}

AddressItem::~AddressItem()
{
	delete ui;
}

void AddressItem::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange)
	{
		ui->retranslateUi(this);

		setName();

		setIdType();
	}

	QWidget::changeEvent(event);
}

void AddressItem::changeNameHeight()
{
	if((width() - reservedWidth) < nameWidth)
	{
		ui->name->setMinimumHeight(LINE_HEIGHT * 2);
		ui->name->setMaximumHeight(LINE_HEIGHT * 2);
		setMinimumHeight(ITEM_HEIGHT + LINE_HEIGHT);
		setMaximumHeight(ITEM_HEIGHT + LINE_HEIGHT);
		enlarged = true;
	}
	else if(enlarged)
	{
		ui->name->setMinimumHeight(LINE_HEIGHT);
		ui->name->setMaximumHeight(LINE_HEIGHT);
		setMinimumHeight(ITEM_HEIGHT);
		setMaximumHeight(ITEM_HEIGHT);
		enlarged = false;
	}

	setName();
}

void AddressItem::disable(bool disable)
{
	setStyleSheet(QString("border: solid rgba(217, 217, 216, 0.45); border-width: 0px 0px 1px 0px;"
		"background-color: %1; color: #000000; text-decoration: none solid rgb(0, 0, 0);")
		.arg(disable ? "#F0F0F0" : "#FFFFFF"));
}

const CKey& AddressItem::getKey() const
{
	return key;
}

void AddressItem::idChanged(const QString& cardCode, const QString& mobileCode)
{
	yourself = (!code.isEmpty() && code == cardCode);
	setName();
}

void AddressItem::mouseReleaseEvent(QMouseEvent *event)
{
	KeyDialog dlg(key, this);
	dlg.exec();
}

void AddressItem::recalculate()
{
	// Reserved width: signature icon (24px) + remove or add buttons + 5px margin before button
	int buttonWidth = ui->icon->isHidden() ? 0 : ICON_WIDTH + 10;
	if(ui->remove->isVisible())
		buttonWidth += (ICON_WIDTH + 5);
	if(ui->add->isVisible())
		buttonWidth += (ADD_WIDTH + 5);
	if(ui->added->isVisible())
		buttonWidth += (ADDED_WIDTH + 5);

	int oldRvWidth = reservedWidth;
	int oldNameWidth = nameWidth;
	reservedWidth = buttonWidth;
	nameWidth = nameMetrics->width(name  + " " + code);

	if(oldRvWidth != reservedWidth || oldNameWidth != nameWidth)
		changeNameHeight();
}

void AddressItem::resizeEvent(QResizeEvent *event)
{
	if(event->oldSize().width() != event->size().width())
		changeNameHeight();
}

void AddressItem::setName()
{
	if(yourself)
		ui->name->setText(QString(ADDRESS_ITEM).arg(name, code + tr(" (Yourself)")));
	else
		ui->name->setText(QString(ADDRESS_ITEM).arg(name, code));
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
	recalculate();	
}

void AddressItem::stateChange(ContainerState state)
{
	if( state & (UnencryptedContainer | DecryptedContainer))
	{
		ui->remove->show();
	}
	else
	{
		ui->remove->hide();
	}
}

void AddressItem::update(const QString& cardName, const QString& cardCode, const QString& type, const QString& strDate, ShowToolButton show)
{
	typeText = type;
	expireDateText = strDate;
	code = cardCode;
	name = cardName;

	setIdType();
	showButton(show);
	changeNameHeight();
}

void AddressItem::setIdType()
{
	QString str;
	QTextStream st(&str);
	st << tr(qPrintable(typeText));

	if(!expireDateText.isEmpty())
	{
		if(!typeText.isEmpty())
			st << " - ";
		st << tr("Expires on") << " " << expireDateText;
	}

	ui->idType->setText(str);
}
