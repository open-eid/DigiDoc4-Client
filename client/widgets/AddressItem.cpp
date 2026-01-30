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

#include <QDebug>
#include <qsslcertificate.h>
#include <qsslkey.h>

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
	CDKey key;
	QString label;
	QDateTime expireDate;
	bool yourself = false;
};

AddressItem::AddressItem(const CDKey &key, Type type, QWidget *parent)
	: Item(parent)
	, ui(new Private)
{
	ui->key = key;
	ui->setupUi(this);
	if(type == Icon)
		ui->icon->load(QStringLiteral(":/images/icon_Krypto_small.svg"));
	ui->icon->setVisible(type == Icon);
	ui->name->setAttribute(Qt::WA_TransparentForMouseEvents, true);
	ui->expire->setAttribute(Qt::WA_TransparentForMouseEvents, true);
	ui->idType->setAttribute(Qt::WA_TransparentForMouseEvents, true);

	bool unsupported = false;
	if (!ui->key.rcpt_cert.isNull()) {
		// Recipient certificate
		ui->code = SslCertificate(ui->key.rcpt_cert).personalCode();
		ui->label = !ui->key.rcpt_cert.subjectInfo("GN").isEmpty() &&
							!ui->key.rcpt_cert.subjectInfo("SN").isEmpty()
						? ui->key.rcpt_cert.subjectInfo("GN").join(' ') + ' ' +
							  ui->key.rcpt_cert.subjectInfo("SN").join(' ')
						: ui->key.rcpt_cert.subjectInfo("CN").join(' ');
		ui->decrypt->hide();
	} else if (ui->key.lock.isValid()) {
		// Known lock type
		ui->code.clear();
		auto map = libcdoc::Recipient::parseLabel(ui->key.lock.label);
		if (map.contains("cn")) {
			ui->label = QString::fromStdString(map["cn"]);
		} else {
			ui->label = QString::fromStdString(ui->key.lock.label);
		}
		if (map.contains("x-expiry-time")) {
			ui->expireDate = QDateTime::fromSecsSinceEpoch(QString::fromStdString(map["x-expiry-time"]).toLongLong());
		}
		if (ui->key.lock.isSymmetric()) {
			ui->decrypt->show();
			connect(ui->decrypt, &QToolButton::clicked, this,
					[this] { emit decrypt(&ui->key.lock); });
		} else {
			ui->decrypt->hide();
		}
	} else {
		// No rcpt, lock is invalid = unsupported lock
		unsupported = true;
		ui->code.clear();
		ui->label = tr("Unsupported cryptographic algorithm or recipient type");
		ui->decrypt->hide();
	}

	if(!unsupported)
		setCursor(Qt::PointingHandCursor);

	connect(ui->add, &QToolButton::clicked, this, [this]{ emit add(this);});
	connect(ui->remove, &QToolButton::clicked, this, [this]{ emit remove(this);});

	setIdType();
	ui->add->setVisible(type == Add);
	ui->remove->setVisible(type != Add);
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
	if (event->type() == QEvent::EnabledChange)
		ui->add->setText(isEnabled() ? tr("Add") : tr("Added"));
	QWidget::changeEvent(event);
}

const CDKey& AddressItem::getKey() const
{
	return ui->key;
}

void AddressItem::idChanged(const SslCertificate &cert) {
	QByteArray qder = cert.toDer();
	std::vector<uint8_t> sder = std::vector<uint8_t>(qder.cbegin(), qder.cend());

	if (ui->key.lock.isValid()) {
		QSslKey pkey = cert.publicKey();
		QByteArray der = pkey.toDer();
		ui->yourself = ui->key.lock.hasTheSameKey(
			std::vector<uint8_t>(der.cbegin(), der.cend()));
	}
	setName();
}

void AddressItem::initTabOrder(QWidget *item)
{
	setTabOrder(item, ui->name);
	setTabOrder(ui->name, ui->idType);
	setTabOrder(ui->idType, ui->expire);
	setTabOrder(ui->expire, ui->remove);
	setTabOrder(ui->remove, lastTabWidget());
}

QWidget* AddressItem::lastTabWidget()
{
	return ui->add;
}

void AddressItem::mouseReleaseEvent(QMouseEvent * /*event*/) {
	if (!ui->key.rcpt_cert.isNull() || ui->key.lock.isCertificate())
		(new KeyDialog(ui->key, this))->open();
}

void AddressItem::setName()
{
	ui->name->setText(QStringLiteral("%1 <span style=\"font-weight:normal;\">%2</span>")
		.arg(ui->label.toHtmlEscaped(), (ui->yourself ? ui->code + tr(" (Yourself)") : ui->code).toHtmlEscaped()));
	if(ui->name->text().isEmpty())
		ui->name->hide();
}

void AddressItem::stateChange(ContainerState state)
{
	if(ui->add->isHidden())
		ui->remove->setVisible(state == UnencryptedContainer);
}

void AddressItem::setIdType(const SslCertificate &cert) {
	SslCertificate::CertType type = cert.type();
	if(type & SslCertificate::DigiIDType)
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
	ui->expire->setProperty("label", QStringLiteral("default"));
	ui->expire->setText(QStringLiteral("%1 %2").arg(
		cert.isValid() ? tr("Expires on") : tr("Expired on"),
		cert.expiryDate().toLocalTime().toString(
			QStringLiteral("dd.MM.yyyy"))));
}

void AddressItem::setIdType() {
	ui->expire->clear();

	if (!ui->key.rcpt_cert.isNull()) {
		// Recipient certificate
		SslCertificate cert(ui->key.rcpt_cert);
		setIdType(cert);
	} else if (ui->key.lock.isValid()) {
		// Known lock type
		// Needed to include translation for "ID-CARD"
		void(QT_TR_NOOP("ID-CARD"));
		auto items = libcdoc::Recipient::parseLabel(ui->key.lock.label);
		if (ui->key.lock.isCertificate()) {
			auto bytes = ui->key.lock.getBytes(libcdoc::Lock::CERT);
			QByteArray qbytes((const char *)bytes.data(), bytes.size());
			SslCertificate cert(qbytes, QSsl::Der);
			setIdType(cert);
		} else {
			ui->idType->setText(tr(items["type"].data()));
		}
		if (ui->key.lock.type == libcdoc::Lock::SERVER) {
			std::string server_exp = items["server_exp"];
			if (!server_exp.empty()) {
				uint64_t seconds = std::stoull(server_exp);
				auto date = QDateTime::fromSecsSinceEpoch(seconds, Qt::UTC);
				bool canDecrypt = QDateTime::currentDateTimeUtc() < date;
				ui->expire->setProperty("label", canDecrypt
													 ? QStringLiteral("good")
													 : QStringLiteral("error"));
				ui->expire->setText(
					canDecrypt ? QStringLiteral("%1 %2").arg(
									 tr("Decryption is possible until:"),
									 date.toLocalTime().toString(
										 QStringLiteral("dd.MM.yyyy")))
							   : tr("Decryption has expired"));
			}
		} else if(!ui->expireDate.isNull()) {
			bool canDecrypt = ui->expireDate > QDateTime::currentDateTime();
			ui->expire->setProperty("label", canDecrypt
												 ? QStringLiteral("good")
												 : QStringLiteral("warn"));
			ui->expire->setText(
				canDecrypt ? QStringLiteral("%1 %2").arg(
								 tr("Decryption is possible until:"),
								 ui->expireDate.toLocalTime().toString(QStringLiteral("dd.MM.yyyy")))
						   : tr("Decryption has expired"));
		}
	} else {
		// No rcpt, lock is invalid = unsupported lock
		ui->idType->setText("Unsupported");
		ui->expire->setHidden(true);
	}
	ui->idType->setHidden(ui->idType->text().isEmpty());
	ui->expire->setHidden(ui->expire->text().isEmpty());
}
