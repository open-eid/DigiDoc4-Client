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


#include "AddRecipients.h"
#include "ui_AddRecipients.h"

#include "Application.h"
#include "CheckConnection.h"
#include "FileDialog.h"
#include "IKValidator.h"
#include "LdapSearch.h"
#include "QSigner.h"
#include "Settings.h"
#include "TokenData.h"
#include "dialogs/WarningDialog.h"
#include "effects/Overlay.h"
#include "widgets/AddressItem.h"
#include "widgets/ItemList.h"

#include <QtCore/QDateTime>
#include <QtCore/QJsonArray>
#include <QtNetwork/QSslConfiguration>
#include <QtNetwork/QSslError>
#include <QtWidgets/QMessageBox>

AddRecipients::AddRecipients(ItemList* itemList, QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::AddRecipients)
	, ldap_corp(new LdapSearch(Application::confValue(QLatin1String("LDAP-CORP-URL")).toString(QStringLiteral("ldaps://k3.ldap.sk.ee")), this))
{
	for(const auto list = Application::confValue(QLatin1String("LDAP-PERSON-URLS")).toArray(); auto url: list) {
		ldap_person.append(new LdapSearch(url.toString(), this));
	}
	if(ldap_person.isEmpty()) {
		ldap_person.append(new LdapSearch(QStringLiteral("ldaps://esteid.ldap.sk.ee"), this));
		ldap_person.append(new LdapSearch(QStringLiteral("ldaps://ldap.eidpki.ee/dc=eidpki,dc=ee"), this));
	}

	ui->setupUi(this);
#if defined (Q_OS_WIN)
	ui->actionLayout->setDirection(QBoxLayout::RightToLeft);
#endif
	setWindowFlags( Qt::Dialog | Qt::CustomizeWindowHint );
	new Overlay(this);

	ui->leftPane->init(ria::qdigidoc4::ToAddAdresses, QT_TRANSLATE_NOOP("ItemList", "Add recipients"));
	ui->rightPane->init(ria::qdigidoc4::AddedAdresses, QT_TRANSLATE_NOOP("ItemList", "Added recipients"));

	connect(ui->confirm, &QPushButton::clicked, this, &AddRecipients::accept);
	connect(ui->cancel, &QPushButton::clicked, this, &AddRecipients::reject);
	connect(ui->leftPane, &ItemList::search, this, [&](const QString &term) {
		ui->leftPane->clear();
		search(term);
	});
	for(auto ldap: ldap_person) {
		connect(ldap, &LdapSearch::searchResult, this, &AddRecipients::showResult);
		connect(ldap, &LdapSearch::error, this, &AddRecipients::showError);
	}
	connect(ldap_corp, &LdapSearch::searchResult, this, &AddRecipients::showResult);
	connect(ldap_corp, &LdapSearch::error, this, &AddRecipients::showError);
	connect(this, &AddRecipients::finished, this, &AddRecipients::close);

	connect(ui->leftPane, &ItemList::addAll, this, [this] {
		for(Item *item: ui->leftPane->items)
			addRecipientToRightPane(item);
	});
	connect(ui->rightPane, &ItemList::removed, ui->rightPane, &ItemList::removeItem );

	connect(ui->fromCard, &QPushButton::clicked, this, [this] {
		addRecipient(qApp->signer()->tokenauth().cert());
	});
	auto enableRecipientFromCard = [this] {
		ui->fromCard->setDisabled(qApp->signer()->tokenauth().cert().isNull());
	};
	enableRecipientFromCard();
	connect(qApp->signer(), &QSigner::authDataChanged, this, std::move(enableRecipientFromCard));

	connect(ui->fromFile, &QPushButton::clicked, this, &AddRecipients::addRecipientFromFile);
	connect(ui->fromHistory, &QPushButton::clicked, this, &AddRecipients::addRecipientFromHistory);

	for(Item *item: itemList->items)
		addRecipientToRightPane(item, false);
}

AddRecipients::~AddRecipients()
{
	QApplication::restoreOverrideCursor();
	delete ui;
}


void AddRecipients::addRecipientFromFile()
{
	QString file = FileDialog::getOpenFileName(this, windowTitle(), {},
		tr("Certificates (*.cer *.crt *.pem)") );
	if( file.isEmpty() )
		return;

	QFile f( file );
	if( !f.open( QIODevice::ReadOnly ) )
	{
		WarningDialog::show(this, tr("Failed to read certificate"));
		return;
	}

	QSslCertificate cert( &f, QSsl::Pem );
	if( cert.isNull() )
	{
		f.reset();
		cert = QSslCertificate( &f, QSsl::Der );
	}
	if( cert.isNull() )
	{
		WarningDialog::show(this, tr("Failed to read certificate"));
	}
	else if( !SslCertificate( cert ).keyUsage().contains( SslCertificate::KeyEncipherment ) &&
		!SslCertificate( cert ).keyUsage().contains( SslCertificate::KeyAgreement ) )
	{
		WarningDialog::show(this, tr("This certificate cannot be used for encryption"));
	}
	else
		addRecipient(cert);
}

void AddRecipients::addRecipientFromHistory()
{
	auto *dlg = new CertificateHistory(historyCertData, this);
	connect(dlg, &CertificateHistory::addSelectedCerts, this, [this](const QList<HistoryCertData> &selectedCertData) {
		if(selectedCertData.isEmpty())
			return;

		ui->leftPane->clear();
		for(const HistoryCertData &certData: selectedCertData) {
			QString term = (certData.type == QLatin1String("1") || certData.type == QLatin1String("3")) ? certData.CN : certData.CN.split(',').value(2);
			search(term, true, certData.type);
		}
	});
	dlg->open();
}

void AddRecipients::addRecipient(const QSslCertificate& cert, bool select)
{
	AddressItem *leftItem = itemListValue(ui->leftPane, cert);
	if(!leftItem)
	{
		leftItem = new AddressItem(cert, AddressItem::Add, ui->leftPane);
		ui->leftPane->addWidget(leftItem);
		leftItem->setDisabled(rightList.contains(cert));
		connect(leftItem, &AddressItem::add, this, [this](Item *item) { addRecipientToRightPane(item); });
		if(auto *add = ui->leftPane->findChild<QWidget*>(QStringLiteral("add")))
			add->setVisible(true);
	}
	if(select)
		addRecipientToRightPane(leftItem);
}

void AddRecipients::addRecipientToRightPane(Item *item, bool update)
{
	auto *address = qobject_cast<AddressItem*>(item);
	if(!address || rightList.contains(address->getKey()))
		return;

	const auto &key = address->getKey();
	if(update)
	{
		if(auto expiryDate = key.cert.expiryDate(); expiryDate <= QDateTime::currentDateTime())
		{
			if(Settings::CDOC2_DEFAULT && Settings::CDOC2_USE_KEYSERVER)
			{
				WarningDialog::show(this, tr("Failed to add certificate. An expired certificate cannot be used for encryption."));
				return;
			}
			auto *dlg = new WarningDialog(tr("Are you sure that you want use certificate for encrypting, which expired on %1?<br />"
				"When decrypter has updated certificates then decrypting is impossible.")
				.arg(expiryDate.toString(QStringLiteral("dd.MM.yyyy hh:mm:ss"))), this);
			dlg->setCancelText(WarningDialog::NO);
			dlg->addButton(WarningDialog::YES, QMessageBox::Yes);
			if(dlg->exec() != QMessageBox::Yes)
				return;
		}
		QSslConfiguration backup = QSslConfiguration::defaultConfiguration();
		QSslConfiguration::setDefaultConfiguration(CheckConnection::sslConfiguration());
		QList<QSslError> errors = QSslCertificate::verify({ key.cert });
		QSslConfiguration::setDefaultConfiguration(backup);
		errors.removeAll(QSslError(QSslError::CertificateExpired, key.cert));
		if(!errors.isEmpty())
		{
			auto *dlg = new WarningDialog(tr("Recipient’s certification chain contains certificates that are not trusted. Continue with encryption?"), this);
			dlg->setCancelText(WarningDialog::NO);
			dlg->addButton(WarningDialog::YES, QMessageBox::Yes);
			if(dlg->exec() != QMessageBox::Yes)
				return;
		}
	}

	rightList.append(key);

	auto *rightItem = new AddressItem(key, AddressItem::Remove, ui->rightPane);
	connect(rightItem, &AddressItem::remove, this, [this](Item *item) {
		auto *rightItem = qobject_cast<AddressItem*>(item);
		if(auto *leftItem = itemListValue(ui->leftPane, rightItem->getKey().cert))
			leftItem->setDisabled(false);
		rightList.removeAll(rightItem->getKey());
		ui->confirm->setDisabled(rightList.isEmpty());
	});
	ui->rightPane->addWidget(rightItem);
	ui->confirm->setEnabled(true);
	historyCertData.addAndSave(key.cert);
	if(auto *leftItem = itemListValue(ui->leftPane, key))
		leftItem->setDisabled(true);
}

bool AddRecipients::isUpdated() const
{
	return ui->confirm->isEnabled();
}

AddressItem* AddRecipients::itemListValue(ItemList *list, const CKey &cert)
{
	for(auto *item: list->items)
	{
		if(auto *address = qobject_cast<AddressItem*>(item); address && address->getKey() == cert)
			return address;
	}
	return nullptr;
}

QList<CKey> AddRecipients::keys()
{
	QList<CKey> recipients;
	for(auto *item: ui->rightPane->items)
	{
		if(auto *address = qobject_cast<AddressItem *>(item))
			recipients.append(address->getKey());
	}
	return recipients;
}

void AddRecipients::search(const QString &term, bool select, const QString &type)
{
	QApplication::setOverrideCursor(Qt::WaitCursor);

	QVariantMap userData {
		{QStringLiteral("type"), type},
		{QStringLiteral("select"), select}
	};
	QString cleanTerm = term.simplified()
#ifdef Q_OS_WIN
		.replace(QStringLiteral("\\"), QStringLiteral("\\5c"))
		.replace(QStringLiteral("*"), QStringLiteral("\\2A"))
		.replace(QStringLiteral("("), QStringLiteral("\\28"))
		.replace(QStringLiteral(")"), QStringLiteral("\\29"));
#else
		.replace(QStringLiteral("\\"), QStringLiteral("\\\\"))
		.replace(QStringLiteral("*"), QStringLiteral("\\*"))
		.replace(QStringLiteral("("), QStringLiteral("\\("))
		.replace(QStringLiteral(")"), QStringLiteral("\\)"));
#endif
	multiSearch = 0;
	bool isDigit = false;
	void(cleanTerm.toULongLong(&isDigit));
	if(!isDigit || (cleanTerm.size() != 11 && cleanTerm.size() != 8))
		ldap_corp->search(QStringLiteral("(cn=*%1*)").arg(cleanTerm), userData);
	else if(cleanTerm.size() == 8)
		ldap_corp->search(QStringLiteral("(serialNumber=%1)" ).arg(cleanTerm), userData);
	else if(IKValidator::isValid(cleanTerm))
	{
		userData[QStringLiteral("personSearch")] = true;
		for(auto *ldap: ldap_person) {
			ldap->search(QStringLiteral("(serialNumber=PNOEE-%1)").arg(cleanTerm), userData);
			++multiSearch;
		}
	}
	else
	{
		QApplication::restoreOverrideCursor();
		WarningDialog::show(this, tr("Personal code is not valid!"));
	}
}

void AddRecipients::showError( const QString &msg, const QString &details )
{
	QApplication::restoreOverrideCursor();
	WarningDialog::show(this, msg, details);
}

void AddRecipients::showResult(const QList<QSslCertificate> &result, int resultCount, const QVariantMap &userData)
{
	for(const QSslCertificate &k: result)
	{
		SslCertificate c(k);
		if((c.keyUsage().contains(SslCertificate::KeyEncipherment) ||
			c.keyUsage().contains(SslCertificate::KeyAgreement)) &&
			!c.enhancedKeyUsage().contains(SslCertificate::ServerAuth) &&
			(userData.value(QStringLiteral("personSearch"), false).toBool() || !c.enhancedKeyUsage().contains(SslCertificate::ClientAuth)) &&
			c.type() != SslCertificate::MobileIDType)
		{
			addRecipient(k, userData.value(QStringLiteral("select"), false).toBool() &&
				(userData.value(QStringLiteral("type")).isNull() || HistoryCertData::toType(SslCertificate(k)) == userData[QStringLiteral("type")]));
		}
	}
	if(resultCount >= 50)
		showError(tr("The name you were looking for gave too many results, please refine your search."));
	else if(--multiSearch <= 0 && ui->leftPane->items.isEmpty())
	{
		showError(tr("Person or company does not own a valid certificate.<br />"
			"It is necessary to have a valid certificate for encryption.<br />"
			"<a href='https://www.id.ee/en/article/encryption-and-decryption-of-documents/'>Read more about it</a>."));
	}
	QApplication::restoreOverrideCursor();
}
