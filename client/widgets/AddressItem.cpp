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

using namespace ria::qdigidoc4;

AddressItem::AddressItem(CKey k, QWidget *parent, bool showIcon)
	: Item(parent)
	, ui(new Ui::AddressItem)
	, key(std::move(k))
{
	ui->setupUi(this);
	if(showIcon)
		ui->icon->load(QStringLiteral(":/images/icon_Krypto_small.svg"));
	ui->icon->setVisible(showIcon);
	ui->name->setFont(Styles::font(Styles::Regular, 14, QFont::DemiBold));
	ui->name->installEventFilter(this);
	ui->idType->setFont(Styles::font(Styles::Regular, 11));

	ui->remove->setIcons(QStringLiteral("/images/icon_remove.svg"), QStringLiteral("/images/icon_remove_hover.svg"),
		QStringLiteral("/images/icon_remove_pressed.svg"), 17, 17);
	ui->remove->init(LabelButton::White, QString(), 0);
	connect(ui->add, &QToolButton::clicked, this, [this]{ emit add(this);});
	connect(ui->remove, &LabelButton::clicked, this, [this]{ emit remove(this);});

	ui->add->setFont(Styles::font(Styles::Condensed, 12));
	ui->added->setFont(Styles::font(Styles::Condensed, 12));

	ui->add->hide();
	ui->added->hide();
	ui->added->setDisabled(true);

	QString name = !key.cert.subjectInfo("GN").isEmpty() && !key.cert.subjectInfo("SN").isEmpty() ?
			key.cert.subjectInfo("GN").join(' ') + " " + key.cert.subjectInfo("SN").join(' ') :
			key.cert.subjectInfo("CN").join(' ');

	QString strDate;
	if(!showIcon)
	{
		DateTime date(key.cert.expiryDate().toLocalTime());
		if(!date.isNull())
			strDate = date.formatDate(QStringLiteral("dd. MMMM yyyy"));
	}

	update(name.toHtmlEscaped(), SslCertificate(key.cert).personalCode(), SslCertificate(key.cert).type(), strDate, AddressItem::Remove);
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

void AddressItem::disable(bool disable)
{
	setStyleSheet(QStringLiteral("border: solid rgba(217, 217, 216, 0.45); border-width: 0px 0px 1px 0px;"
		"background-color: %1; color: #000000; text-decoration: none solid rgb(0, 0, 0);")
		.arg(disable ? QStringLiteral("#F0F0F0") : QStringLiteral("#FFFFFF")));
}

bool AddressItem::eventFilter(QObject *o, QEvent *e)
{
	if(o == ui->name && e->type() == QEvent::MouseButtonRelease)
		KeyDialog(key, this).exec();
	return Item::eventFilter(o, e);
}

const CKey& AddressItem::getKey() const
{
	return key;
}

void AddressItem::idChanged(const QString& cardCode, const QString& /*mobileCode*/, const QByteArray& serialNumber)
{
	yourself = (!code.isEmpty() && code == cardCode) && (serialNumber == key.cert.serialNumber());

	setName();
}

QWidget* AddressItem::initTabOrder(QWidget *item)
{
	setTabOrder(item, ui->name);
	setTabOrder(ui->name, ui->idType);
	setTabOrder(ui->idType, ui->remove);
	setTabOrder(ui->remove, ui->added);
	setTabOrder(ui->added, ui->add);
	return ui->add;
}

void AddressItem::mouseReleaseEvent(QMouseEvent * /*event*/)
{
	KeyDialog(key, this).exec();
}

void AddressItem::setName()
{
	ui->name->setText(QStringLiteral("%1 <span style=\"font-weight:normal;\">%2</span>").arg(name, yourself ? code + tr(" (Yourself)") : code));
}

void AddressItem::showButton(ShowToolButton show)
{
	ui->remove->setVisible(show == Remove);
	ui->add->setVisible(show == Add);
	ui->added->setVisible(show == Added);
}

void AddressItem::stateChange(ContainerState state)
{
	ui->remove->setVisible(state == UnencryptedContainer);
}

void AddressItem::update(const QString& cardName, const QString& cardCode, SslCertificate::CertType type, const QString& strDate, ShowToolButton show)
{
	m_type = type;
	expireDateText = strDate;
	code = cardCode;
	name = cardName;

	setIdType();
	showButton(show);
}

void AddressItem::setIdType()
{
	QString str;
	if(m_type & SslCertificate::DigiIDType)
		str = tr("Digi-ID");
	else if(m_type & SslCertificate::EstEidType)
		str = tr("ID-card");
	else if(m_type & SslCertificate::TempelType)
		str = tr("Certificate for Encryption");
	else if(m_type & SslCertificate::MobileIDType)
		str = tr("Mobile-ID");

	if(!expireDateText.isEmpty())
	{
		if(!str.isEmpty())
			str += " - ";
		str += tr("Expires on") + " " + expireDateText;
	}

	ui->idType->setText(str);
}
