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
#include "SslCertificate.h"
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
	ui->name->setAttribute(Qt::WA_TransparentForMouseEvents, true);
	ui->expire->setAttribute(Qt::WA_TransparentForMouseEvents, true);
	ui->idType->setAttribute(Qt::WA_TransparentForMouseEvents, true);
	if(!ui->key.unsupported)
		setCursor(Qt::PointingHandCursor);

	connect(ui->add, &QToolButton::clicked, this, [this]{ emit add(this);});
	connect(ui->remove, &QToolButton::clicked, this, [this]{ emit remove(this);});

	ui->code = SslCertificate(ui->key.cert).personalCode();
	ui->label = !ui->key.cert.subjectInfo("GN").isEmpty() && !ui->key.cert.subjectInfo("SN").isEmpty() ?
			ui->key.cert.subjectInfo("GN").join(' ') + ' ' + ui->key.cert.subjectInfo("SN").join(' ') :
			ui->key.cert.subjectInfo("CN").join(' ');
	if(ui->label.isEmpty())
		ui->label = ui->key.fromKeyLabel().value(QStringLiteral("cn"), ui->key.recipient);
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

const CKey& AddressItem::getKey() const
{
	return ui->key;
}

void AddressItem::idChanged(const SslCertificate &cert)
{
	CKey key(cert);
	ui->yourself = !key.key.isNull() && ui->key == key;
	setName();
}

void AddressItem::initTabOrder(QWidget *item)
{
	setTabOrder(item, ui->name);
	setTabOrder(ui->name, ui->idType);
	setTabOrder(ui->idType, ui->expire);
	setTabOrder(ui->expire, ui->remove);
	setTabOrder(ui->remove, ui->added);
	setTabOrder(ui->added, lastTabWidget());
}

QWidget* AddressItem::lastTabWidget()
{
	return ui->add;
}

void AddressItem::mouseReleaseEvent(QMouseEvent * /*event*/)
{
	if(!ui->key.unsupported)
		(new KeyDialog(ui->key, this))->open();
}

void AddressItem::setName()
{
	ui->name->setText(QStringLiteral("%1 <span style=\"font-weight:normal;\">%2</span>")
		.arg(ui->label.toHtmlEscaped(), (ui->yourself ? ui->code + tr(" (Yourself)") : ui->code).toHtmlEscaped()));
	if(ui->name->text().isEmpty())
		ui->name->hide();
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
	ui->expire->clear();
	SslCertificate cert(ui->key.cert);
	SslCertificate::CertType type = cert.type();
	if(ui->key.unsupported)
	{
		ui->label = tr("Unsupported cryptographic algorithm or recipient type");
		ui->idType->clear();
	}
	else if(type & SslCertificate::DigiIDType)
		ui->idType->setText(tr("digi-ID"));
	else if(type & SslCertificate::EstEidType)
		ui->idType->setText(tr("ID-card"));
	else if(type & SslCertificate::MobileIDType)
		ui->idType->setText(tr("mobile-ID"));
	else if(type & SslCertificate::TempelType)
	{
		if(cert.keyUsage().contains(SslCertificate::NonRepudiation))
			ui->idType->setText(tr("e-Seal"));
		else if(cert.enhancedKeyUsage().contains(SslCertificate::ClientAuth))
			ui->idType->setText(tr("Authentication certificate"));
		else
			ui->idType->setText(tr("Certificate for Encryption"));
	}
	else
	{
		auto items = ui->key.fromKeyLabel();
		void(QT_TR_NOOP("ID-CARD"));
		ui->idType->setText(tr(items[QStringLiteral("type")].toUtf8().data()));
		if(QString server_exp = items[QStringLiteral("server_exp")]; !server_exp.isEmpty())
		{
			auto date = QDateTime::fromSecsSinceEpoch(server_exp.toLongLong(), Qt::UTC);
			bool canDecrypt = QDateTime::currentDateTimeUtc() < date;
			ui->expire->setProperty("label", canDecrypt ? QStringLiteral("good") : QStringLiteral("error"));
			ui->expire->setText(canDecrypt ? QStringLiteral("%1 %2").arg(
				tr("Decryption is possible until:"), date.toLocalTime().toString(QStringLiteral("dd.MM.yyyy"))) :
				tr("Decryption has expired"));
		}
	}

	if(!cert.isNull())
	{
		ui->expire->setProperty("label", QStringLiteral("default"));
		ui->expire->setText(QStringLiteral("%1 %2").arg(
			cert.isValid() ? tr("Expires on") : tr("Expired on"),
			cert.expiryDate().toLocalTime().toString(QStringLiteral("dd.MM.yyyy"))));
	}

	ui->idType->setHidden(ui->idType->text().isEmpty());
	ui->expire->setHidden(ui->expire->text().isEmpty());
}
