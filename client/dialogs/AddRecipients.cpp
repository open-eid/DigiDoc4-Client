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
#include "common_enums.h"
#include "FileDialog.h"
#include "QSigner.h"
#include "Styles.h"
#include "TokenData.h"
#include "crypto/LdapSearch.h"
#include "dialogs/WarningDialog.h"
#include "effects/Overlay.h"

#include <common/Configuration.h>
#include <common/IKValidator.h>

#include <QDebug>
#include <QDateTime>
#include <QSslKey>
#include <QStandardPaths>
#include <QtCore/QJsonObject>
#include <QtCore/QXmlStreamReader>
#include <QtCore/QXmlStreamWriter>
#include <QtCore/QSettings>
#include <QMessageBox>
#include <QtNetwork/QSslError>

AddRecipients::AddRecipients(ItemList* itemList, QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::AddRecipients)
	, ldap_person(new LdapSearch(defaultUrl(QStringLiteral("LDAP-PERSON-URL"), QStringLiteral("ldaps://esteid.ldap.sk.ee")).toUtf8(), this))
	, ldap_corp(new LdapSearch(defaultUrl(QStringLiteral("LDAP-CORP-URL"), QStringLiteral("ldaps://k3.ldap.sk.ee")).toUtf8(), this))
{
	ui->setupUi(this);
	setWindowFlags( Qt::Dialog | Qt::CustomizeWindowHint );
	new Overlay(this, parent->topLevelWidget());

	ui->leftPane->init(ria::qdigidoc4::ToAddAdresses, QStringLiteral("Add recipients"));
	ui->leftPane->setFont(Styles::font(Styles::Regular, 20));
	ui->rightPane->init(ria::qdigidoc4::AddedAdresses, QStringLiteral("Added recipients"));
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

	QFile f( path() );
	if( !f.open( QIODevice::ReadOnly ) )
		return;

	QXmlStreamReader xml( &f );
	if( !xml.readNextStartElement() || xml.name() != "History" )
		return;

	while( xml.readNextStartElement() )
	{
		if( xml.name() == "item" )
		{
			historyCertData.append({
				xml.attributes().value( "CN" ).toString(),
				xml.attributes().value( "type" ).toString(),
				xml.attributes().value( "issuer" ).toString(),
				xml.attributes().value( "expireDate" ).toString()
			});
		}
		xml.skipCurrentElement();
	}

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
	QList<HistoryCertData> history;

	for(AddressItem *value: leftList)
	{
		if(!rightList.contains(value->getKey().cert))
		{
			addRecipientToRightPane(value);
			history << toHistory(value->getKey().cert);
		}
	}
	ui->confirm->setDisabled(rightList.isEmpty());
	rememberCerts(history);
}

void AddRecipients::addRecipientFromCard()
{
	if(AddressItem *item = addRecipientToLeftPane(qApp->signer()->tokenauth().cert()))
		addRecipientToRightPane(item, true);
}

void AddRecipients::addRecipientFromFile()
{
	QString file = FileDialog::getOpenFileName( this, windowTitle(), QString(),
		tr("Certificates (*.cer *.crt *.pem)") );
	if( file.isEmpty() )
		return;

	QFile f( file );
	if( !f.open( QIODevice::ReadOnly ) )
	{
		WarningDialog::warning(this, tr("Failed to read certificate"));
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
		WarningDialog::warning( this, tr("Failed to read certificate"));
	}
	else if( !SslCertificate( cert ).keyUsage().contains( SslCertificate::KeyEncipherment ) &&
		!SslCertificate( cert ).keyUsage().contains( SslCertificate::KeyAgreement ) )
	{
		WarningDialog::warning( this, tr("This certificate cannot be used for encryption"));
	}
	else if(AddressItem *item = addRecipientToLeftPane(cert))
	{
		addRecipientToRightPane(item, true);
	}
	f.close();
}

void AddRecipients::addRecipientFromHistory()
{
	CertificateHistory dlg(historyCertData, this);
	connect(&dlg, &CertificateHistory::addSelectedCerts, this, &AddRecipients::addSelectedCerts);
	connect(&dlg, &CertificateHistory::removeSelectedCerts, this, &AddRecipients::removeSelectedCerts);
	dlg.exec();
}

AddressItem * AddRecipients::addRecipientToLeftPane(const QSslCertificate& cert)
{
	AddressItem *leftItem = leftList.value(cert);
	if(leftItem)
		return leftItem;

	leftItem = new AddressItem(CKey(cert), ui->leftPane);
	leftList.insert(cert, leftItem);
	ui->leftPane->addWidget(leftItem);
	bool contains = rightList.contains(cert);
	leftItem->disable(contains);
	leftItem->showButton(contains ? AddressItem::Added : AddressItem::Add);

	connect(leftItem, &AddressItem::add, this, [this](Item *item) {
		addRecipientToRightPane(static_cast<AddressItem*>(item), true);
	});

	if(QWidget *add = ui->leftPane->findChild<QWidget*>(QStringLiteral("add")))
		add->setVisible(true);

	return leftItem;
}

bool AddRecipients::addRecipientToRightPane(const CKey &key, bool update)
{
	if (rightList.contains(key.cert))
		return false;

	if(update)
	{
		auto expiryDate = key.cert.expiryDate();
		if(expiryDate <= QDateTime::currentDateTime())
		{
			WarningDialog dlg(tr("Are you sure that you want use certificate for encrypting, which expired on %1?<br />"
								 "When decrypter has updated certificates then decrypting is impossible.")
								  .arg(expiryDate.toString(QStringLiteral("dd.MM.yyyy hh:mm:ss"))), this);
			dlg.setCancelText(tr("NO"));
			dlg.addButton(tr("YES"), QMessageBox::Yes);
			if(dlg.exec() != QMessageBox::Yes)
				return false;
		}
		QList<QSslError> errors = QSslCertificate::verify({ key.cert });
		errors.removeAll(QSslError(QSslError::CertificateExpired, key.cert));
		if(!errors.isEmpty())
		{
			WarningDialog dlg(tr("Recipient’s certification chain contains certificates that are not trusted. Continue with encryption?"), this);
			dlg.setCancelText(tr("NO"));
			dlg.addButton(tr("YES"), QMessageBox::Yes);
			if(dlg.exec() != QMessageBox::Yes)
				return false;
		}
	}
	updated = update;

	rightList.append(key.cert);

	AddressItem *rightItem = new AddressItem(key, ui->rightPane);
	ui->rightPane->addWidget(rightItem);
	rightItem->showButton(AddressItem::Remove);

	connect(rightItem, &AddressItem::remove, this, &AddRecipients::removeRecipientFromRightPane );
	ui->confirm->setDisabled(rightList.isEmpty());
	rememberCerts({toHistory(key.cert)});
	return true;
}

void AddRecipients::addRecipientToRightPane(AddressItem *leftItem, bool update)
{
	if(addRecipientToRightPane(leftItem->getKey(), update)) {
		leftItem->disable(true);
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

QString AddRecipients::defaultUrl(const QString &key, const QString &defaultValue)
{
#ifdef CONFIG_URL
	return Configuration::instance().object().value(key).toString(defaultValue);
#else
	Q_UNUSED(key)
	return defaultValue;
#endif
}

void AddRecipients::enableRecipientFromCard()
{
	ui->fromCard->setDisabled( qApp->signer()->tokenauth().cert().isNull() );
}

bool AddRecipients::isUpdated()
{
	return updated;
}

QList<CKey> AddRecipients::keys()
{
	QList<CKey> recipients;
	for(auto item: ui->rightPane->items)
	{
		if(AddressItem *address = qobject_cast<AddressItem *>(item))
			recipients << address->getKey();
	}
	return recipients;
}

QString AddRecipients::path() const
{
#ifdef Q_OS_WIN
	QSettings s( QSettings::IniFormat, QSettings::UserScope, qApp->organizationName(), "qdigidoccrypto" );
	QFileInfo f( s.fileName() );
	return f.absolutePath() + "/" + f.baseName() + "/certhistory.xml";
#else
	return QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/certhistory.xml";
#endif
}

void AddRecipients::rememberCerts(const QList<HistoryCertData>& selectedCertData)
{
	if(selectedCertData.isEmpty())
		return;

	for(const auto &certData: selectedCertData)
	{
		if(!historyCertData.contains(certData))
			historyCertData << certData;
	}

	saveHistory();
}

void AddRecipients::removeRecipientFromRightPane(Item *toRemove)
{
	AddressItem *rightItem = static_cast<AddressItem *>(toRemove);
	auto it = leftList.find(rightItem->getKey().cert);
	if(it != leftList.end())
	{
		it.value()->disable(false);
		it.value()->showButton(AddressItem::Add);
	}
	rightList.removeAll(rightItem->getKey().cert);
	updated = true;
	ui->confirm->setDisabled(rightList.isEmpty());
}

void AddRecipients::removeSelectedCerts(const QList<HistoryCertData>& removeCertData)
{
	if(removeCertData.isEmpty())
		return;


	QMutableListIterator<HistoryCertData> i(historyCertData);
	while (i.hasNext())
	{
		if(removeCertData.contains(i.next()))
			i.remove();
	}

	saveHistory();
}

void AddRecipients::saveHistory()
{
	QString p = path();
	QDir().mkpath( QFileInfo( p ).absolutePath() );
	QFile f( p );
	if( !f.open( QIODevice::WriteOnly|QIODevice::Truncate ) )
		return;

	QXmlStreamWriter xml( &f );
	xml.setAutoFormatting( true );
	xml.writeStartDocument();
	xml.writeStartElement(QStringLiteral("History"));
	for(const HistoryCertData& certData : historyCertData)
	{
		xml.writeStartElement(QStringLiteral("item"));
		xml.writeAttribute(QStringLiteral("CN"), certData.CN);
		xml.writeAttribute(QStringLiteral("type"), certData.type);
		xml.writeAttribute(QStringLiteral("issuer"), certData.issuer);
		xml.writeAttribute(QStringLiteral("expireDate"), certData.expireDate);
		xml.writeEndElement();
	}
	xml.writeEndDocument();
}

void AddRecipients::search(const QString &term, bool select, const QString &type)
{
	QApplication::setOverrideCursor(Qt::WaitCursor);
	ui->confirm->setDefault(false);
	ui->confirm->setAutoDefault(false);
	QRegExp isDigit( "\\d*" );

	QVariantMap userData {
		{"type", type},
		{"select", select}
	};
	QString cleanTerm = term.simplified();
	if(isDigit.exactMatch(cleanTerm) && (cleanTerm.size() == 11 || cleanTerm.size() == 8))
	{
		if(cleanTerm.size() == 11)
		{
			if(!IKValidator::isValid(cleanTerm))
			{
				QApplication::restoreOverrideCursor();
				WarningDialog::warning(this, tr("Personal code is not valid!"));
				return;
			}
			userData["personSearch"] = true;
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
	WarningDialog(msg, details, this).exec();
}

void AddRecipients::showResult(const QList<QSslCertificate> &result, int resultCount, const QVariantMap &userData)
{
	QList<QSslCertificate> filter;
	for(const QSslCertificate &k: result)
	{
		SslCertificate c(k);
		if((c.keyUsage().contains(SslCertificate::KeyEncipherment) ||
			c.keyUsage().contains(SslCertificate::KeyAgreement)) &&
			!c.enhancedKeyUsage().contains(SslCertificate::ServerAuth) &&
			(userData.value("personSearch", false).toBool() || !c.enhancedKeyUsage().contains(SslCertificate::ClientAuth)) &&
			c.type() != SslCertificate::MobileIDType)
			filter << c;
	}
	if(filter.isEmpty())
	{
		showError(tr("Person or company does not own a valid certificate.<br />"
			"It is necessary to have a valid certificate for encryption.<br />"
			"In case of questions please contact our support via <a href=\"https://www.id.ee/en/\">id.ee</a>."));
	}
	else
	{
		for(const QSslCertificate &k: filter)
		{
			AddressItem *item = addRecipientToLeftPane(k);
			if(userData.value(QStringLiteral("select"), false).toBool() &&
				(userData.value(QStringLiteral("type")).isNull() || toType(SslCertificate(k)) == userData[QStringLiteral("type")]))
				addRecipientToRightPane(item, true);
		}
	}

	if(resultCount >= 50)
	{
		showError(tr("The name you were looking for gave a very large number of results, not all of the search results will be shown. "
			"If the desired recipient is not on the list, please enter a more specific name in the search box."));
	}

	QApplication::restoreOverrideCursor();
}

HistoryCertData AddRecipients::toHistory(const QSslCertificate& c) const
{
	HistoryCertData hcd;
	SslCertificate cert(c);
	hcd.CN = cert.subjectInfo("CN");
	hcd.type = toType(cert);
	hcd.issuer = cert.issuerInfo("CN");
	hcd.expireDate = cert.expiryDate().toLocalTime().toString(QStringLiteral("dd.MM.yyyy"));
	return hcd;
}

QString AddRecipients::toType(const SslCertificate &cert) const
{
	auto certType = cert.type();
	if(certType & SslCertificate::DigiIDType) return QString::number(2);
	if(certType & SslCertificate::TempelType) return QString::number(1);
	if(certType & SslCertificate::EstEidType) return QString::number(0);
	return QString::number(3);
}
