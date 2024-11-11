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
#include "dialogs/PasswordDialog.h"
#include "ui_AddRecipients.h"

#include "Application.h"
#include "CheckConnection.h"
#include "common_enums.h"
#include "FileDialog.h"
#include "IKValidator.h"
#include "LdapSearch.h"
#include "QSigner.h"
#include "Settings.h"
#include "Styles.h"
#include "TokenData.h"
#include "dialogs/WarningDialog.h"
#include "effects/Overlay.h"
#include "Crypto.h"

#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtNetwork/QSslConfiguration>
#include <QtNetwork/QSslError>
#include <QtWidgets/QMessageBox>

AddRecipients::AddRecipients(ItemList* itemList, QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::AddRecipients)
	, ldap_person(new LdapSearch(defaultUrl(QLatin1String("LDAP-PERSON-URL"), QStringLiteral("ldaps://esteid.ldap.sk.ee")).toUtf8(), this))
	, ldap_corp(new LdapSearch(defaultUrl(QLatin1String("LDAP-CORP-URL"), QStringLiteral("ldaps://k3.ldap.sk.ee")).toUtf8(), this))
{
	ui->setupUi(this);
#if defined (Q_OS_WIN)
	ui->actionLayout->setDirection(QBoxLayout::RightToLeft);
#endif
	setWindowFlags( Qt::Dialog | Qt::CustomizeWindowHint );
	new Overlay(this);

	ui->leftPane->init(ria::qdigidoc4::ToAddAdresses, QT_TRANSLATE_NOOP("ItemList", "Add recipients"));
	ui->leftPane->setFont(Styles::font(Styles::Regular, 20));
	ui->rightPane->init(ria::qdigidoc4::AddedAdresses, QT_TRANSLATE_NOOP("ItemList", "Added recipients"));
	ui->rightPane->setFont(Styles::font(Styles::Regular, 20));

	ui->fromCard->setFont(Styles::font(Styles::Condensed, 12));
	ui->fromFile->setFont(Styles::font(Styles::Condensed, 12));
	ui->fromHistory->setFont(Styles::font(Styles::Condensed, 12));

	ui->cancel->setFont(Styles::font(Styles::Condensed, 14));
	ui->confirm->setFont(Styles::font(Styles::Condensed, 14));

	ui->confirm->setDisabled(rightList.isEmpty());
	connect(ui->confirm, &QPushButton::clicked, this, &AddRecipients::accept);
	connect(ui->cancel, &QPushButton::clicked, this, &AddRecipients::reject);
	connect(ui->leftPane, &ItemList::search, this, [&](const QString &term) {
		leftList.clear();
		ui->leftPane->clear();
		search(term);
	});
	connect(ldap_person, &LdapSearch::searchResult, this, &AddRecipients::showResult);
	connect(ldap_corp, &LdapSearch::searchResult, this, &AddRecipients::showResult);
	connect(ldap_person, &LdapSearch::error, this, &AddRecipients::showError);
	connect(ldap_corp, &LdapSearch::error, this, &AddRecipients::showError);
	connect(this, &AddRecipients::finished, this, &AddRecipients::close);

	connect(ui->leftPane, &ItemList::addAll, this, &AddRecipients::addAllRecipientToRightPane );
	connect(ui->rightPane, &ItemList::removed, ui->rightPane, &ItemList::removeItem );

	connect(ui->fromCard, &QPushButton::clicked, this, &AddRecipients::addRecipientFromCard);
	connect( qApp->signer(), &QSigner::authDataChanged, this, &AddRecipients::enableRecipientFromCard );
	enableRecipientFromCard();

	connect(ui->fromFile, &QPushButton::clicked, this, &AddRecipients::addRecipientFromFile);
	connect(ui->fromHistory, &QPushButton::clicked, this, &AddRecipients::addRecipientFromHistory);

	for(Item *item: itemList->items)
		addRecipientToRightPane((qobject_cast<AddressItem *>(item))->getKey(), false);
}

AddRecipients::~AddRecipients()
{
	QApplication::restoreOverrideCursor();
	delete ui;
}

void AddRecipients::addAllRecipientToRightPane()
{
	QList<SslCertificate> history;
	for(AddressItem *value: leftList)
	{
		if(rightList.contains(value->getKey()))
			continue;
		addRecipientToRightPane(value);
		std::shared_ptr<CKey> key = value->getKey();
		if (key->type == CKey::Type::CDOC1) {
			std::shared_ptr<CKeyCDoc1> kd = std::static_pointer_cast<CKeyCDoc1>(key);
			history.append(kd->cert);
		}
	}
	ui->confirm->setDisabled(rightList.isEmpty());
	historyCertData.addAndSave(history);
}

void AddRecipients::addRecipientFromCard()
{
	if(auto *item = addRecipientToLeftPane(qApp->signer()->tokenauth().cert()))
		addRecipientToRightPane(item, true);
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
	else if(auto *item = addRecipientToLeftPane(cert))
	{
		addRecipientToRightPane(item, true);
	}
}

void AddRecipients::addRecipientFromHistory()
{
	auto *dlg = new CertificateHistory(historyCertData, this);
	connect(dlg, &CertificateHistory::addSelectedCerts, this, &AddRecipients::addSelectedCerts);
	dlg->open();
}

AddressItem * AddRecipients::addRecipientToLeftPane(const QSslCertificate& cert)
{
	AddressItem *leftItem = leftList.value(cert);
	if(leftItem)
		return leftItem;

	leftItem = new AddressItem(std::make_shared<CKeyCert>(cert), ui->leftPane);
	leftList.insert(cert, leftItem);
	ui->leftPane->addWidget(leftItem);

	bool contains = false;
	for (std::shared_ptr<CKey> k: rightList) {
		if (k->isTheSameRecipient(cert)) {
			contains = true;
			break;
		}
	}
	leftItem->setDisabled(contains);
	leftItem->showButton(contains ? AddressItem::Added : AddressItem::Add);

	connect(leftItem, &AddressItem::add, this, [this](Item *item) {
		addRecipientToRightPane(qobject_cast<AddressItem*>(item), true);
	});

	if(auto *add = ui->leftPane->findChild<QWidget*>(QStringLiteral("add")))
		add->setVisible(true);

	return leftItem;
}

bool AddRecipients::addRecipientToRightPane(std::shared_ptr<CKey> key, bool update)
{
	for (std::shared_ptr<CKey> k: rightList) {
		if (k->isTheSameRecipient(*key)) return false;
	}

	if(update)
	{
		if (key->type == CKey::Type::CDOC1) {
			std::shared_ptr<CKeyCDoc1> kd = std::static_pointer_cast<CKeyCDoc1>(key);
			if(auto expiryDate = kd->cert.expiryDate(); expiryDate <= QDateTime::currentDateTime())
			{
				if(Settings::CDOC2_DEFAULT && Settings::CDOC2_USE_KEYSERVER)
				{
					WarningDialog::show(this, tr("Failed to add certificate. An expired certificate cannot be used for encryption."));
					return false;
				}
				auto *dlg = new WarningDialog(tr("Are you sure that you want use certificate for encrypting, which expired on %1?<br />"
					"When decrypter has updated certificates then decrypting is impossible.")
					.arg(expiryDate.toString(QStringLiteral("dd.MM.yyyy hh:mm:ss"))), this);
				dlg->setCancelText(WarningDialog::NO);
				dlg->addButton(WarningDialog::YES, QMessageBox::Yes);
				if(dlg->exec() != QMessageBox::Yes)
					return false;
			}
			QSslConfiguration backup = QSslConfiguration::defaultConfiguration();
			QSslConfiguration::setDefaultConfiguration(CheckConnection::sslConfiguration());
			QList<QSslError> errors = QSslCertificate::verify({ kd->cert });
			QSslConfiguration::setDefaultConfiguration(backup);
			errors.removeAll(QSslError(QSslError::CertificateExpired, kd->cert));
			if(!errors.isEmpty())
			{
				auto *dlg = new WarningDialog(tr("Recipient’s certification chain contains certificates that are not trusted. Continue with encryption?"), this);
				dlg->setCancelText(WarningDialog::NO);
				dlg->addButton(WarningDialog::YES, QMessageBox::Yes);
				if(dlg->exec() != QMessageBox::Yes)
					return false;
			}
		}
	}
	updated = update;

	rightList.append(key);

	auto *rightItem = new AddressItem(key, ui->rightPane);
	connect(rightItem, &AddressItem::remove, this, &AddRecipients::removeRecipientFromRightPane);
	ui->rightPane->addWidget(rightItem);
	ui->confirm->setDisabled(rightList.isEmpty());
	if (key->type == CKey::Type::CDOC1) {
		std::shared_ptr<CKeyCDoc1> kd = std::static_pointer_cast<CKeyCDoc1>(key);
		historyCertData.addAndSave({kd->cert});
	}
	return true;
}

void AddRecipients::addRecipientToRightPane(AddressItem *leftItem, bool update)
{
	if(addRecipientToRightPane(leftItem->getKey(), update)) {
		leftItem->setDisabled(true);
		leftItem->showButton(AddressItem::Added);
	}
}

void AddRecipients::addSelectedCerts(const QList<HistoryCertData>& selectedCertData)
{
	if(selectedCertData.isEmpty())
		return;

	leftList.clear();
	ui->leftPane->clear();
	for(const HistoryCertData &certData: selectedCertData) {
		QString term = (certData.type == QStringLiteral("1") || certData.type == QStringLiteral("3")) ? certData.CN : certData.CN.split(',').value(2);
		search(term, true, certData.type);
	}
}

QString AddRecipients::defaultUrl(QLatin1String key, const QString &defaultValue)
{
	return Application::confValue(key).toString(defaultValue);
}

void AddRecipients::enableRecipientFromCard()
{
	ui->fromCard->setDisabled( qApp->signer()->tokenauth().cert().isNull() );
}

bool AddRecipients::isUpdated() const
{
	return updated;
}

QList<std::shared_ptr<CKey>> AddRecipients::keys()
{
	QList<std::shared_ptr<CKey>> recipients;
	for(auto *item: ui->rightPane->items)
	{
		if(auto *address = qobject_cast<AddressItem *>(item))
			recipients.append(address->getKey());
	}
	return recipients;
}

void AddRecipients::removeRecipientFromRightPane(Item *toRemove)
{
	auto *rightItem = qobject_cast<AddressItem*>(toRemove);
	std::shared_ptr<CKey> key = rightItem->getKey();
	if (key->type == CKey::Type::CDOC1) {
		std::shared_ptr<CKeyCDoc1> kd = std::static_pointer_cast<CKeyCDoc1>(key);
		if(auto it = leftList.find(kd->cert); it != leftList.end())
		{
			it.value()->setDisabled(false);
			it.value()->showButton(AddressItem::Add);
		}
	}
	rightList.removeAll(rightItem->getKey());
	updated = true;
	ui->confirm->setDisabled(rightList.isEmpty());
}

void AddRecipients::search(const QString &term, bool select, const QString &type)
{
	QApplication::setOverrideCursor(Qt::WaitCursor);
	ui->confirm->setDefault(false);
	ui->confirm->setAutoDefault(false);

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
	bool isDigit = false;
	void(cleanTerm.toULongLong(&isDigit));
	if(isDigit && (cleanTerm.size() == 11 || cleanTerm.size() == 8))
	{
		if(cleanTerm.size() == 11)
		{
			if(!IKValidator::isValid(cleanTerm))
			{
				QApplication::restoreOverrideCursor();
				WarningDialog::show(this, tr("Personal code is not valid!"));
				return;
			}
			userData[QStringLiteral("personSearch")] = true;
			ldap_person->search(QStringLiteral("(serialNumber=%1%2)" ).arg(ldap_person->isSSL() ? QStringLiteral("PNOEE-") : QString(), cleanTerm), userData);
		}
		else
			ldap_corp->search(QStringLiteral("(serialNumber=%1)" ).arg(cleanTerm), userData);
	}
	else
	{
		ldap_corp->search(QStringLiteral("(cn=*%1*)").arg(cleanTerm), userData);
	}
}

void AddRecipients::showError( const QString &msg, const QString &details )
{
	QApplication::restoreOverrideCursor();
	WarningDialog::show(this, msg, details);
}

void AddRecipients::showResult(const QList<QSslCertificate> &result, int resultCount, const QVariantMap &userData)
{
	bool isEmpty = true;
	for(const QSslCertificate &k: result)
	{
		SslCertificate c(k);
		if((c.keyUsage().contains(SslCertificate::KeyEncipherment) ||
			c.keyUsage().contains(SslCertificate::KeyAgreement)) &&
			!c.enhancedKeyUsage().contains(SslCertificate::ServerAuth) &&
			(userData.value(QStringLiteral("personSearch"), false).toBool() || !c.enhancedKeyUsage().contains(SslCertificate::ClientAuth)) &&
			c.type() != SslCertificate::MobileIDType)
		{
			isEmpty = false;
			AddressItem *item = addRecipientToLeftPane(k);
			if(userData.value(QStringLiteral("select"), false).toBool() &&
				(userData.value(QStringLiteral("type")).isNull() || HistoryCertData::toType(SslCertificate(k)) == userData[QStringLiteral("type")]))
				addRecipientToRightPane(item, true);
		}
	}
	if(resultCount >= 50)
		showError(tr("The name you were looking for gave too many results, please refine your search."));
	else if(isEmpty)
	{
		showError(tr("Person or company does not own a valid certificate.<br />"
			"It is necessary to have a valid certificate for encryption.<br />"
			"<a href='https://www.id.ee/en/article/encryption-and-decryption-of-documents/'>Read more about it</a>."));
	}
	QApplication::restoreOverrideCursor();
}
