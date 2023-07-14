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

#include "CryptoDoc.h"
#include "DateTime.h"
#include "SslCertificate.h"
#include "Styles.h"
#include "dialogs/KeyDialog.h"

using namespace ria::qdigidoc4;

class AddressItem::Private: public Ui::AddressItem
{
public:
	QString code;
	CKey key;
	QString label;
	bool yourself = false;
};

AddressItem::AddressItem(CKey k, QWidget *parent, bool showIcon)
	: Item(parent)
	, ui(new Private)
{
	ui->key = std::move(k);
	ui->setupUi(this);
	if(showIcon)
		ui->icon->load(QStringLiteral(":/images/icon_Krypto_small.svg"));
	ui->icon->setVisible(showIcon);
	ui->name->setFont(Styles::font(Styles::Regular, 14, QFont::DemiBold));
	ui->name->installEventFilter(this);
	ui->idType->setFont(Styles::font(Styles::Regular, 11));
	ui->idType->installEventFilter(this);

	ui->remove->setIcons(QStringLiteral("/images/icon_remove.svg"), QStringLiteral("/images/icon_remove_hover.svg"),
		QStringLiteral("/images/icon_remove_pressed.svg"), 17, 17);
	ui->remove->init(LabelButton::White, {}, 0);
	connect(ui->add, &QToolButton::clicked, this, [this]{ emit add(this);});
	connect(ui->remove, &LabelButton::clicked, this, [this]{ emit remove(this);});

	ui->add->setFont(Styles::font(Styles::Condensed, 12));
	ui->added->setFont(ui->add->font());

	ui->code = SslCertificate(ui->key.cert).personalCode().toHtmlEscaped();
	ui->label = (!ui->key.cert.subjectInfo("GN").isEmpty() && !ui->key.cert.subjectInfo("SN").isEmpty() ?
			ui->key.cert.subjectInfo("GN").join(' ') + " " + ui->key.cert.subjectInfo("SN").join(' ') :
			ui->key.cert.subjectInfo("CN").join(' ')).toHtmlEscaped();
	if(ui->label.isEmpty())
		ui->label = ui->key.recipient;
	setIdType();
	showButton(AddressItem::Remove);
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

bool AddressItem::eventFilter(QObject *o, QEvent *e)
{
	if((o == ui->name || o == ui->idType) && e->type() == QEvent::MouseButtonRelease)
	{
		(new KeyDialog(ui->key, this))->open();
		return true;
	}
	return Item::eventFilter(o, e);
}

const CKey& AddressItem::getKey() const
{
	return ui->key;
}

void AddressItem::idChanged(const CKey &key)
{
	ui->yourself = !key.key.isNull() && ui->key == key;
	setName();
}

void AddressItem::idChanged(const SslCertificate &cert)
{
	idChanged(CKey(cert));
}

void AddressItem::initTabOrder(QWidget *item)
{
	setTabOrder(item, ui->name);
	setTabOrder(ui->name, ui->idType);
	setTabOrder(ui->idType, ui->remove);
	setTabOrder(ui->remove, ui->added);
	setTabOrder(ui->added, lastTabWidget());
}

QWidget* AddressItem::lastTabWidget()
{
	return ui->add;
}

void AddressItem::mouseReleaseEvent(QMouseEvent * /*event*/)
{
	(new KeyDialog(ui->key, this))->open();
}

void AddressItem::setName()
{
	ui->name->setText(QStringLiteral("%1 <span style=\"font-weight:normal;\">%2</span>")
		.arg(ui->label, ui->yourself ? ui->code + tr(" (Yourself)") : ui->code));
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

void AddressItem::setIdType()
{
	ui->idType->setHidden(ui->key.cert.isNull());
	if(ui->key.cert.isNull())
		return;

	QString str;
	SslCertificate cert(ui->key.cert);
	SslCertificate::CertType type = cert.type();
	if(type & SslCertificate::DigiIDType)
		str = tr("digi-ID");
	else if(type & SslCertificate::EstEidType)
		str = tr("ID-card");
	else if(type & SslCertificate::MobileIDType)
		str = tr("mobile-ID");
	else if(type & SslCertificate::TempelType)
	{
		if(cert.keyUsage().contains(SslCertificate::NonRepudiation))
			str = tr("e-Seal");
		else if(cert.enhancedKeyUsage().contains(SslCertificate::ClientAuth))
			str = tr("Authentication certificate");
		else
			str = tr("Certificate for Encryption");
	}

	if(!str.isEmpty())
		str += QStringLiteral(" - ");
	DateTime date(cert.expiryDate().toLocalTime());
	ui->idType->setText(QStringLiteral("%1%2 %3").arg(str,
		cert.isValid() ? tr("Expires on") : tr("Expired on"),
		date.formatDate(QStringLiteral("dd. MMMM yyyy"))));
}
