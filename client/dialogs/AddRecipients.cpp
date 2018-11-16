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
#include "crypto/LdapSearch.h"
#include "dialogs/WarningDialog.h"
#include "effects/Overlay.h"

#include <common/Settings.h>
#include <common/SslCertificate.h>
#include <common/TokenData.h>
#include <common/IKValidator.h>

#include <QDebug>
#include <QDateTime>
#include <QSslKey>
#include <QStandardPaths>
#include <QtCore/QXmlStreamReader>
#include <QtCore/QXmlStreamWriter>
#include <QMessageBox>
#include <QtNetwork/QSslError>

AddRecipients::AddRecipients(ItemList* itemList, QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::AddRecipients)
	, ldap_person(new LdapSearch(qApp->confValue(Application::LDAP_PERSON_URL).toByteArray(), this))
	, ldap_corp(new LdapSearch(qApp->confValue(Application::LDAP_CORP_URL).toByteArray(), this))
{
	ui->setupUi(this);
	setWindowFlags( Qt::Dialog | Qt::CustomizeWindowHint );
	setWindowModality( Qt::ApplicationModal );

	ui->leftPane->init(ria::qdigidoc4::ToAddAdresses, tr("Add recipients"));
	ui->leftPane->setFont(Styles::font(Styles::Regular, 20));
	ui->rightPane->init(ria::qdigidoc4::AddedAdresses, tr("Added recipients"));
	ui->rightPane->setFont(Styles::font(Styles::Regular, 20));

	ui->fromCard->setFont(Styles::font(Styles::Condensed, 12));
	ui->fromFile->setFont(Styles::font(Styles::Condensed, 12));
	ui->fromHistory->setFont(Styles::font(Styles::Condensed, 12));

	ui->cancel->setFont(Styles::font(Styles::Condensed, 14));
	ui->confirm->setFont(Styles::font(Styles::Condensed, 14));

	ui->confirm->setDisabled(rightList.isEmpty());
	connect(ui->confirm, &QPushButton::clicked, this, &AddRecipients::accept);
	connect(ui->cancel, &QPushButton::clicked, this, &AddRecipients::reject);
	connect(ui->leftPane, &ItemList::search, this, &AddRecipients::search);
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
			historyCertData.append(
				{
					xml.attributes().value( "CN" ).toString(),
					xml.attributes().value( "type" ).toString(),
					xml.attributes().value( "issuer" ).toString(),
					xml.attributes().value( "expireDate" ).toString()
				});
		}
		xml.skipCurrentElement();
	}

	initAddressItems(itemList->items);
}

AddRecipients::~AddRecipients()
{
	QApplication::restoreOverrideCursor();
	delete ui;
}

void AddRecipients::addAllRecipientToRightPane()
{
	QList<HistoryCertData> history;

	for(auto it = leftList.begin(); it != leftList.end(); ++it)
	{
		if(!rightList.contains(it.key()))
		{
			addRecipientToRightPane(it.value());
			history << toHistory(it.value()->getKey().cert);
		}
	}
	ui->confirm->setDisabled(rightList.isEmpty());
	rememberCerts(history);
}

void AddRecipients::addRecipientFromCard()
{
	if(Item *item = addRecipientToLeftPane(qApp->signer()->tokenauth().cert()))
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
	else
	{
		Item *item = addRecipientToLeftPane( cert );
		if( item )
			addRecipientToRightPane( item, true );
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
	QString friendlyName = SslCertificate(cert).friendlyName();
	if(leftList.contains(friendlyName))
		return nullptr;

	AddressItem *leftItem = new AddressItem(CKey(cert), ui->leftPane);
	leftList.insert(friendlyName, leftItem);
	ui->leftPane->addWidget(leftItem);
	if(rightList.contains(friendlyName))
	{
		leftItem->disable(true);
		leftItem->showButton(AddressItem::Added);
	}
	else
	{
		leftItem->disable(false);
		leftItem->showButton(AddressItem::Add);
	}

	connect(leftItem, &AddressItem::add, this, [this](Item *item) {
		addRecipientToRightPane(item, true);
	});

	return leftItem;
}

void AddRecipients::addRecipientToRightPane(Item *toAdd, bool update)
{
	AddressItem *leftItem = qobject_cast<AddressItem *>(toAdd);

	if (rightList.contains(SslCertificate(leftItem->getKey().cert).friendlyName()))
		return;

	if(update)
	{
		auto expiryDate = leftItem->getKey().cert.expiryDate();
		if(expiryDate <= QDateTime::currentDateTime())
		{
			WarningDialog dlg(tr("Are you sure that you want use certificate for encrypting, which expired on %1?<br />"
					"When decrypter has updated certificates then decrypting is impossible.")
					.arg(expiryDate.toString(QStringLiteral("dd.MM.yyyy hh:mm:ss"))), this);
			dlg.setCancelText(tr("NO"));
			dlg.addButton(tr("YES"), QMessageBox::Yes);
			dlg.exec();
			if(dlg.result() != QMessageBox::Yes)
				return;
		}
		QList<QSslError> errors = QSslCertificate::verify(QList<QSslCertificate>() << leftItem->getKey().cert);
		errors.removeAll(QSslError(QSslError::CertificateExpired, leftItem->getKey().cert));
		if(!errors.isEmpty())
		{
			WarningDialog dlg(tr("Recipient’s certification chain contains certificates that are not trusted. Continue with encryption?"), this);
			dlg.setCancelText(tr("NO"));
			dlg.addButton(tr("YES"), QMessageBox::Yes);
			dlg.exec();
			if(dlg.result() != QMessageBox::Yes)
				return;
		}
	}
	updated = update;

	rightList.append(SslCertificate(leftItem->getKey().cert).friendlyName());

	AddressItem *rightItem = new AddressItem(leftItem->getKey(), ui->leftPane);
	ui->rightPane->addWidget(rightItem);
	rightItem->showButton(AddressItem::Remove);

	connect(rightItem, &AddressItem::remove, this, &AddRecipients::removeRecipientFromRightPane );
	leftItem->disable(true);
	leftItem->showButton(AddressItem::Added);
	ui->confirm->setDisabled(rightList.isEmpty());
	rememberCerts({toHistory(leftItem->getKey().cert)});
}

void AddRecipients::addSelectedCerts(const QList<HistoryCertData>& selectedCertData)
{
	if(selectedCertData.isEmpty())
		return;

	HistoryCertData certData = selectedCertData.first();
	QString term = (certData.type == QStringLiteral("1") || certData.type == QStringLiteral("3")) ? certData.CN : certData.CN.split(',').value(2);
	ui->leftPane->setTerm(term);
	search(term);
	select = true;
}

void AddRecipients::enableRecipientFromCard()
{
	ui->fromCard->setDisabled( qApp->signer()->tokenauth().cert().isNull() );
}

int AddRecipients::exec()
{
	Overlay overlay(parentWidget());
	overlay.show();
	auto rc = QDialog::exec();
	overlay.close();

	return rc;
}

void AddRecipients::initAddressItems(const std::vector<Item *> &items)
{
	for(Item *item: items)
	{
		AddressItem *leftItem = new AddressItem((qobject_cast<AddressItem *>(item))->getKey(), ui->leftPane);
		QString friendlyName = SslCertificate(leftItem->getKey().cert).friendlyName();

		// Add to left pane
		leftList.insert(friendlyName, leftItem);
		ui->leftPane->addWidget(leftItem);

		leftItem->disable(true);
		leftItem->showButton(AddressItem::Added);
		connect(leftItem, &AddressItem::add, this, [this](Item *item){ addRecipientToRightPane(item, true); });

		// Add to right pane
		addRecipientToRightPane(leftItem, false);
	}
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
	QString friendlyName = SslCertificate(rightItem->getKey().cert).friendlyName();

	auto it = leftList.find(friendlyName);
	if(it != leftList.end())
	{
		it.value()->disable(false);
		it.value()->showButton(AddressItem::Add);
	}
	rightList.removeAll(friendlyName);
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

void AddRecipients::search(const QString &term)
{
	QApplication::setOverrideCursor(Qt::WaitCursor);
	ui->confirm->setDefault(false);
	ui->confirm->setAutoDefault(false);
	QRegExp isDigit( "\\d*" );

	select = false;
	personSearch = false;
	if( isDigit.exactMatch(term) && (term.size() == 11 || term.size() == 8))
	{
		if(term.size() == 11)
		{
			if(!IKValidator::isValid(term))
			{
				QApplication::restoreOverrideCursor();
				WarningDialog::warning(this, tr("Personal code is not valid!"));
				return;
			}
			personSearch = true;
			ldap_person->search(QStringLiteral("(serialNumber=%1%2)" ).arg(ldap_person->isSSL() ? QStringLiteral("PNOEE-") : QString(), term));
		}
		else
			ldap_corp->search(QStringLiteral("(serialNumber=%1)" ).arg(term));
	}
	else
	{
		ldap_corp->search(QStringLiteral("(cn=*%1*)").arg(term));
	}
}

void AddRecipients::showError( const QString &msg, const QString &details )
{
	QApplication::restoreOverrideCursor();
	WarningDialog b(msg, details, this);
	b.exec();
}

void AddRecipients::showResult(const QList<QSslCertificate> &result)
{
	QList<QSslCertificate> filter;
	for(const QSslCertificate &k: result)
	{
		SslCertificate c(k);
		
		if((c.keyUsage().contains(SslCertificate::KeyEncipherment) ||
			c.keyUsage().contains(SslCertificate::KeyAgreement)) &&
			!c.enhancedKeyUsage().contains(SslCertificate::ServerAuth) &&
			(personSearch || !c.enhancedKeyUsage().contains(SslCertificate::ClientAuth)) &&
			c.type() != SslCertificate::MobileIDType)
			filter << c;
	}
	if(filter.isEmpty())
	{
		showError(tr("Person or company does not own a valid certificate.<br />"
			"It is necessary to have a valid certificate for encryption.<br />"
			"In case of questions please contact our support via <a href=\"https://id.ee/?lang=en\">id.ee</a>."));
	}
	else
	{
		leftList.clear();
		ui->leftPane->clear();
		Item *item = nullptr;

		for(const QSslCertificate &k: filter)
		{
			auto address = addRecipientToLeftPane(k);
			if(!item)
				item = address;
		}

		if(select && item)
			addRecipientToRightPane(item, true);
	}

	QApplication::restoreOverrideCursor();
}

HistoryCertData AddRecipients::toHistory(const QSslCertificate& c) const
{
	HistoryCertData hcd;
	int type = 3;
	SslCertificate cert(c);
	auto certType = cert.type();

	if(certType & SslCertificate::DigiIDType)
		type = 2;
	else if(certType & SslCertificate::TempelType)
		type = 1;
	else if(certType & SslCertificate::EstEidType)
		type = 0;

	hcd.CN = cert.subjectInfo("CN");
	hcd.type = QString::number(type);
	hcd.issuer = cert.issuerInfo("CN");
	hcd.expireDate = cert.expiryDate().toLocalTime().toString(QStringLiteral("dd.MM.yyyy"));

	return hcd;
}
