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
#include <QStandardPaths>
#include <QtCore/QXmlStreamReader>
#include <QtCore/QXmlStreamWriter>
#include <QSslKey>

AddRecipients::AddRecipients(const std::vector<Item *> &items, QWidget *parent) :
	QDialog(parent)
	, ui(new Ui::AddRecipients)
	, ldap(new LdapSearch(this))
	, leftList()
	, rightList()
{
	init();
	setAddressItems(items);
}

AddRecipients::~AddRecipients()
{
	delete ui;
}

void AddRecipients::init()
{
	ui->setupUi(this);
	setWindowFlags( Qt::Dialog | Qt::FramelessWindowHint );
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

	connect(ui->confirm, &QPushButton::clicked, this, &AddRecipients::accept);
	connect(ui->cancel, &QPushButton::clicked, this, &AddRecipients::reject);
	connect(ui->leftPane, &ItemList::search, this, &AddRecipients::search);
	connect(ldap, &LdapSearch::searchResult, this, &AddRecipients::showResult);
	connect(ldap, &LdapSearch::error, this, &AddRecipients::showError);
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
			QString type;
			switch (xml.attributes().value( "type" ).toInt())
			{
				case SslCertificate::DigiIDType:
					type = tr("Digi-ID");
					break;
				case SslCertificate::EstEidType:
					type = tr("ID-card");
					break;
				case SslCertificate::MobileIDType:
					type = tr("Mobile-ID");
					break;
				default:
					type = tr("Unknown ID");
					break;
			}

			historyCertData.append(
				{
					xml.attributes().value( "CN" ).toString(),
					type,
					xml.attributes().value( "issuer" ).toString(),
					xml.attributes().value( "expireDate" ).toString()
				});
		}
		xml.skipCurrentElement();
	}

}

void AddRecipients::setAddressItems(const std::vector<Item *> &items)
{
	for(Item *item :items)
	{
		AddressItem *leftItem = new AddressItem((static_cast<AddressItem *>(item))->getKey(),  ria::qdigidoc4::UnencryptedContainer, ui->leftPane);
		QString friendlyName = SslCertificate(leftItem->getKey().cert).friendlyName();

		// Add to left pane
		leftList.insert(friendlyName, leftItem);
		ui->leftPane->addWidget(leftItem);

		leftItem->disable(true);
		leftItem->showButton(AddressItem::Added);
		connect(leftItem, &AddressItem::add, this, &AddRecipients::addRecipientToRightPane );

		// Add to right pane
		addRecipientToRightPane(leftItem);
	}
}

void AddRecipients::enableRecipientFromCard()
{
	ui->fromCard->setDisabled( qApp->signer()->tokenauth().cert().isNull() );
}

void AddRecipients::addRecipientToRightPane(Item *toAdd)
{
	AddressItem *leftItem = static_cast<AddressItem *>(toAdd);
	AddressItem *rightItem = new AddressItem(leftItem->getKey(),  ria::qdigidoc4::UnencryptedContainer, ui->leftPane);

	rightList.append(SslCertificate(leftItem->getKey().cert).friendlyName());
	ui->rightPane->addWidget(rightItem);
	rightItem->showButton(AddressItem::Remove);

	connect(rightItem, &AddressItem::remove, this, &AddRecipients::removeRecipientFromRightPane );
	leftItem->disable(true);
	leftItem->showButton(AddressItem::Added);
}

void AddRecipients::addAllRecipientToRightPane()
{
	for(auto it = leftList.begin(); it != leftList.end(); ++it )
	{
		if(!rightList.contains(it.key()))
		{
			addRecipientToRightPane(it.value());
		}
	}
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
		rightList.removeAll(friendlyName);
	}
}

void AddRecipients::addRecipientToLeftPane(const QSslCertificate& cert)
{
	QString friendlyName = SslCertificate(cert).friendlyName();
	if(!leftList.contains(friendlyName) )
	{
		AddressItem *leftItem = new AddressItem(CKey(cert),  ria::qdigidoc4::UnencryptedContainer, ui->leftPane);

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

		connect(leftItem, &AddressItem::add, this, &AddRecipients::addRecipientToRightPane );
	}
}

void AddRecipients::addRecipientFromCard()
{
	QList<QSslCertificate> certs;

	certs << qApp->signer()->tokenauth().cert();
	for(QSslCertificate& cert : certs)
	{
		addRecipientToLeftPane(cert);
	}
}

void AddRecipients::addRecipientFromFile()
{
	QString file = FileDialog::getOpenFileName( this, windowTitle(), QString(),
		"Certificates (*.pem *.cer *.crt)" );
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
	else if( !SslCertificate( cert ).keyUsage().contains( SslCertificate::KeyEncipherment ) )
	{
		WarningDialog::warning( this, tr("This certificate cannot be used for encryption"));
	}
	else
		addRecipientToLeftPane(cert);

	f.close();
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


void AddRecipients::addRecipientFromHistory()
{
	CertificateHistory dlg(historyCertData, this);
	connect(&dlg, &CertificateHistory::addSelectedCerts, this, &AddRecipients::addSelectedCerts);
	connect(&dlg, &CertificateHistory::removeSelectedCerts, this, &AddRecipients::removeSelectedCerts);
	dlg.exec();
}

void AddRecipients::addSelectedCerts(const QList<HistoryCertData>& selectedCertData)
{
	// TODO
	qDebug() << "addSelectedCerts() NOT IMPLEMENTED";
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


	QString p = path();
	QDir().mkpath( QFileInfo( p ).absolutePath() );
	QFile f( p );
	if( !f.open( QIODevice::WriteOnly|QIODevice::Truncate ) )
		return;

	QXmlStreamWriter xml( &f );
	xml.setAutoFormatting( true );
	xml.writeStartDocument();
	xml.writeStartElement( "History" );
	for(const HistoryCertData& certData : historyCertData)
	{
		xml.writeStartElement( "item" );
		xml.writeAttribute( "CN", certData.CN );
		xml.writeAttribute( "type", certData.type );
		xml.writeAttribute( "issuer", certData.issuer );
		xml.writeAttribute( "expireDate", certData.expireDate );
		xml.writeEndElement();
	}
	xml.writeEndDocument();
}


int AddRecipients::exec()
{
	Overlay overlay(parentWidget());
	overlay.show();
	auto rc = QDialog::exec();
	overlay.close();

	return rc;
}

void AddRecipients::search(const QString &term)
{
	QRegExp isDigit( "\\d*" );
	qDebug() << "Search pressed";
	if( isDigit.exactMatch(term) && ( term.size() == 11 || term.size() == 8 ) )
	{
		if( term.size() == 11 && !IKValidator::isValid( term ) )
		{
			WarningDialog::warning(this, tr("Personal code is not valid!"));
			return;
		}
		ldap->search( QString( "(serialNumber=%1)" ).arg( term ) );
	}
	else
	{
		ldap->search( QString( "(cn=*%1*)" ).arg( term ) );
	}
}

void AddRecipients::showError( const QString &msg, const QString &details )
{
	// disableSearch(false);
	WarningDialog b(msg, details, this);
	b.exec();
}

void AddRecipients::showResult(const QList<QSslCertificate> &result)
{
	QList<QSslCertificate> filter;
	for(const QSslCertificate &k: result)
	{
		SslCertificate c(k);
/* TODO Filter is commented out temporary !!!

		if((c.keyUsage().contains(SslCertificate::KeyEncipherment) ||
			c.keyUsage().contains(SslCertificate::KeyAgreement)) &&
			!c.enhancedKeyUsage().contains(SslCertificate::ServerAuth) &&
			// (searchType->currentIndex() == 0 || !c.enhancedKeyUsage().contains(SslCertificate::ClientAuth)) &&
			c.type() != SslCertificate::MobileIDType &&
			c.publicKey().algorithm() == QSsl::Rsa)
*/			filter << c;
	}
	if(filter.isEmpty())
	{
		showError( tr("Person or company does not own a valid certificate.<br />"
			"It is necessary to have a valid certificate for encryption.<br />"
			"Contact for assistance by email <a href=\"mailto:abi@id.ee\">abi@id.ee</a> "
			"or call 1777 (only from Estonia), (+372) 677 3377 when calling from abroad.") );
	}
	else
	{
		leftList.clear();
		ui->leftPane->clear();
		for(const QSslCertificate &k: filter)
		{
			addRecipientToLeftPane(k);
		}
	}
}
