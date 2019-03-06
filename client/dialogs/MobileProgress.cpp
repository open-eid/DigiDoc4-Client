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

#include "MobileProgress.h"

#include "AccessCert.h"
#include "Application.h"
#include "DigiDoc.h"
#include "Styles.h"

#include <common/Configuration.h>
#include <common/Settings.h>
#include <common/SOAPDocument.h>
#include <common/SslCertificate.h>

#include <QtCore/QDir>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QTimeLine>
#include <QtCore/QTimer>
#include <QtCore/QXmlStreamReader>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkProxy>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSslKey>
#include <QtNetwork/QSslConfiguration>
#ifdef Q_OS_WIN
#include <QtWinExtras/QWinTaskbarButton>
#include <QtWinExtras/QWinTaskbarProgress>
#endif

using namespace digidoc;

MobileProgress::MobileProgress( QWidget *parent )
	: QDialog(parent)
{
	mobileResults[QStringLiteral("START")] = tr("Signing in process");
	mobileResults[QStringLiteral("REQUEST_OK")] = tr("Request accepted");
	mobileResults[QStringLiteral("EXPIRED_TRANSACTION")] = tr("Request timeout");
	mobileResults[QStringLiteral("USER_CANCEL")] = tr("User denied or cancelled");
	mobileResults[QStringLiteral("SIGNATURE")] = tr("Got signature");
	mobileResults[QStringLiteral("OUTSTANDING_TRANSACTION")] = tr("Request pending");
	mobileResults[QStringLiteral("MID_NOT_READY")] = tr("Mobile-ID not ready, try again later");
	mobileResults[QStringLiteral("PHONE_ABSENT")] = tr("Phone absent");
	mobileResults[QStringLiteral("SENDING_ERROR")] = tr("Request sending error");
	mobileResults[QStringLiteral("SIM_ERROR")] = tr("SIM error");
	mobileResults[QStringLiteral("INTERNAL_ERROR")] = tr("Service internal error");
	mobileResults[QStringLiteral("OCSP_UNAUTHORIZED")] = tr("Not allowed to use OCSP service! Please check your server access certificate.");
	mobileResults[QStringLiteral("HOSTNOTFOUND")] = tr("Connecting to SK server failed! Please check your internet connection.");
	mobileResults[QStringLiteral("Invalid PhoneNo")] = tr("Invalid phone number! Please include correct country code.");
	mobileResults[QStringLiteral("User is not a Mobile-ID client")] = tr("User is not a Mobile-ID client");
	mobileResults[QStringLiteral("ID and phone number do not match")] = tr("ID and phone number do not match");
	mobileResults[QStringLiteral("Certificate status unknown")] = tr("Your Mobile-ID service is not activated.");
	mobileResults[QStringLiteral("Certificate is revoked")] = tr("Mobile-ID user certificates are revoked or suspended.");

	setupUi( this );
	code->setBuddy( signProgressBar );

	setWindowFlags( Qt::Dialog | Qt::CustomizeWindowHint );
	setWindowModality( Qt::ApplicationModal );

	statusTimer = new QTimeLine( signProgressBar->maximum() * 1000, this );
	statusTimer->setCurveShape( QTimeLine::LinearCurve );
	statusTimer->setFrameRange( signProgressBar->minimum(), signProgressBar->maximum() );
	connect(statusTimer, &QTimeLine::frameChanged, signProgressBar, &QProgressBar::setValue);
	connect(statusTimer, &QTimeLine::finished, this, [this] { endProgress(mobileResults.value(QStringLiteral("EXPIRED_TRANSACTION"))); });
#ifdef Q_OS_WIN
	taskbar = new QWinTaskbarButton(this);
	taskbar->setWindow(parent->windowHandle());
	taskbar->progress()->setRange(signProgressBar->minimum(), signProgressBar->maximum());
	connect(statusTimer, &QTimeLine::frameChanged, taskbar->progress(), &QWinTaskbarProgress::setValue);
	connect(cancel, &QPushButton::clicked, this, &MobileProgress::stop);
#endif

	QFont condensed14 = Styles::font( Styles::Condensed, 14 );
	QFont regular14 = Styles::font( Styles::Regular, 14 );
	QFont header = Styles::font( Styles::Regular, 20, QFont::DemiBold );
	code->setFont( header );
	labelError->setFont( regular14 );
	signProgressBar->setFont( regular14 );

	cancel->setFont( condensed14 );
	connect(cancel, &QPushButton::clicked, this, &MobileProgress::reject);

	manager = new QNetworkAccessManager( this );
	connect(manager, &QNetworkAccessManager::finished, this, &MobileProgress::finished);
	connect(manager, &QNetworkAccessManager::sslErrors, this, &MobileProgress::sslErrors);

	QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();
#ifdef CONFIG_URL
	for(const QJsonValue &cert: Configuration::instance().object().value(QStringLiteral("CERT-BUNDLE")).toArray())
		trusted << QSslCertificate(QByteArray::fromBase64(cert.toString().toLatin1()), QSsl::Der);
	ssl.setCaCertificates(QList<QSslCertificate>());
#endif
	if( !Application::confValue( Application::PKCS12Disable ).toBool() )
	{
		ssl.setPrivateKey( AccessCert::key() );
		ssl.setLocalCertificate( AccessCert::cert() );
	}
	request.setSslConfiguration( ssl );

	request.setHeader( QNetworkRequest::ContentTypeHeader, "text/xml" );
	request.setRawHeader( "User-Agent", QStringLiteral( "%1/%2 (%3)")
		.arg(qApp->applicationName(), qApp->applicationVersion(), Common::applicationOs()).toUtf8() );
}

void MobileProgress::endProgress(const QString &msg)
{
	labelError->setText(msg);
	signProgressBar->hide();
	stop();
}

void MobileProgress::finished( QNetworkReply *reply )
{
	QScopedPointer<QNetworkReply,QScopedPointerDeleteLater> d(reply);
	switch( reply->error() )
	{
	case QNetworkReply::NoError:
		if(!reply->header(QNetworkRequest::ContentTypeHeader).toByteArray().contains("text/xml"))
		{
			endProgress(tr("Invalid content"));
			return;
		}
	case QNetworkReply::UnknownContentError:
#if QT_VERSION >= QT_VERSION_CHECK(5,3,0)
	case QNetworkReply::InternalServerError:
#endif
		break;
	case QNetworkReply::HostNotFoundError:
		endProgress(mobileResults.value(QStringLiteral("HOSTNOTFOUND")));
		return;
	case QNetworkReply::SslHandshakeFailedError:
		signProgressBar->hide();
		stop();
		return;
	default:
		endProgress(reply->errorString());
		return;
	}

	QXmlStreamReader xml( reply );
	QString fault, message, status;
	while( xml.readNext() != QXmlStreamReader::Invalid )
	{
		if( !xml.isStartElement() )
			continue;
		if( xml.name() == "faultstring" )
			fault = xml.readElementText();
		else if( xml.name() == "message" )
			message = xml.readElementText();
		else if( xml.name() == "ChallengeID" )
		{
			code->setText( tr("Make sure control code matches with one in phone screen\n"
				"and enter Mobile-ID PIN2-code.\nControl code: %1").arg( xml.readElementText() ) );
			code->setAccessibleName( code->text() );
			adjustSize();
		}
		else if( xml.name() == "Sesscode" )
			sessionCode = xml.readElementText();
		else if( xml.name() == "Status" )
			status = xml.readElementText();
		else if( xml.name() == "Signature" )
			m_signature = xml.readElementText().toUtf8();
	}

	if( !fault.isEmpty() )
	{
		endProgress(mobileResults.value(message, message));
		return;
	}

	if( sessionCode.isEmpty() )
	{
		endProgress(mobileResults.value(message));
		return;
	}

	if( statusTimer->state() == QTimeLine::NotRunning )
		return;

	labelError->setText( mobileResults.value( status ) );
	if(status == QStringLiteral("OK") || status == QStringLiteral("REQUEST_OK") || status == QStringLiteral("OUTSTANDING_TRANSACTION"))
	{
		QTimer::singleShot(5*1000, this, &MobileProgress::sendStatusRequest);
		return;
	}
	stop();
	if(status == QStringLiteral("SIGNATURE"))
		accept();
}

bool MobileProgress::isTest( const QString &ssid, const QString &cell )
{
	const static QStringList list = {
		"1421212802037200002",
		"1421212802137200003",
		"1421212802237200004",
		"1421212802337200005",
		"1421212802437200006",
		"1421212802537200007",
		"1421212802637200008",
		"1421212802737200009",
		"3800224021137200001",
		"1421212802937200001066",
	};
	return list.contains( ssid + cell );
}

void MobileProgress::sendStatusRequest()
{
	SOAPDocument doc(QStringLiteral("GetMobileCreateSignatureStatus"), DIGIDOCSERVICE);
	doc.writeParameter(QStringLiteral("Sesscode"), sessionCode.toInt());
	doc.writeParameter(QStringLiteral("WaitSignature"), false);
	doc.writeEndDocument();
	manager->post( request, doc.document() );
}

void MobileProgress::setSignatureInfo( const QString &city, const QString &state,
	const QString &zip, const QString &country, const QString &role )
{
	location = QStringList({city, state, zip, country, role});
}

void MobileProgress::sign( const DigiDoc *doc, const QString &ssid, const QString &cell )
{
	labelError->setText(mobileResults.value(QStringLiteral("START")));

	QHash<QString,QString> lang;
	lang[QStringLiteral("et")] = QStringLiteral("EST");
	lang[QStringLiteral("en")] = QStringLiteral("ENG");
	lang[QStringLiteral("ru")] = QStringLiteral("RUS");

	SOAPDocument r(QStringLiteral("MobileCreateSignature"), DIGIDOCSERVICE );
	r.writeParameter(QStringLiteral("IDCode"), ssid );
	r.writeParameter(QStringLiteral("PhoneNo"), "+" + cell );
	r.writeParameter(QStringLiteral("Language"), lang.value(Settings::language(), QStringLiteral("EST")));
	r.writeParameter(QStringLiteral("ServiceName"), QStringLiteral("DigiDoc3"));
	QString title =  tr("Sign") + " " + QFileInfo( doc->fileName() ).fileName();
	if( title.size() > 39 )
	{
		title.resize( 36 );
		title += QStringLiteral("...");
	}
	r.writeParameter(QStringLiteral("MessageToDisplay"), title + "\n");
	r.writeParameter(QStringLiteral("City"), location.value(0));
	r.writeParameter(QStringLiteral("StateOrProvince"), location.value(1));
	r.writeParameter(QStringLiteral("PostalCode"), location.value(2));
	r.writeParameter(QStringLiteral("CountryName"), location.value(3));
	r.writeParameter(QStringLiteral("Role"), location.value(4));
	r.writeParameter(QStringLiteral("SigningProfile"), QStringLiteral("LT"));

	r.writeStartElement(QStringLiteral("DataFiles"));
	r.writeAttribute(XML_SCHEMA_INSTANCE, QStringLiteral("type"), QStringLiteral("m:DataFileDigestList"));
	DocumentModel *m = doc->documentModel();
	for( int i = 0; i < m->rowCount(); ++i )
	{
		r.writeStartElement(QStringLiteral("DataFileDigest"));
		r.writeAttribute(XML_SCHEMA_INSTANCE, QStringLiteral("type"), QStringLiteral("m:DataFileDigest") );
		r.writeParameter(QStringLiteral("Id"), m->fileId(i) );
		r.writeParameter(QStringLiteral("DigestType"), "sha256");
		r.writeParameter(QStringLiteral("DigestValue"), doc->getFileDigest(i).toBase64());
		r.writeParameter(QStringLiteral("MimeType"), m->mime(i));
		r.writeEndElement();
	}
	r.writeEndElement();

	r.writeParameter(QStringLiteral("Format"), QStringLiteral("BDOC"));
	r.writeParameter(QStringLiteral("Version"), QStringLiteral("2.1"));
	r.writeParameter(QStringLiteral("SignatureID"), doc->newSignatureID());
	r.writeParameter(QStringLiteral("MessagingMode"), QStringLiteral("asynchClientServer"));
	r.writeParameter(QStringLiteral("AsyncConfiguration"), 0);
	r.writeEndDocument();

	request.setUrl(QUrl(qApp->confValue(isTest( ssid, cell ) ? Application::MobileID_TEST_URL : Application::MobileID_URL).toString()));
	statusTimer->start();
#ifdef Q_OS_WIN
	taskbar->progress()->show();
	taskbar->progress()->resume();
#endif
	manager->post( request, r.document() );
}

QByteArray MobileProgress::signature() const { return m_signature; }

void MobileProgress::sslErrors(QNetworkReply *reply, const QList<QSslError> &err)
{
	QStringList msg;
	QList<QSslError> ignore;
	for(const QSslError &e: err)
	{
		switch(e.error())
		{
		case QSslError::UnableToGetLocalIssuerCertificate:
		case QSslError::CertificateUntrusted:
			if(trusted.contains(reply->sslConfiguration().peerCertificate())) {
				ignore << e;
				break;
			}
		default:
			qWarning() << tr("SSL Error:") << e.error() << e.certificate().subjectInfo( "CN" );
			msg << e.errorString();
			break;
		}
	}
	reply->ignoreSslErrors(ignore);
	if( !msg.empty() )
	{
		msg.prepend( tr("SSL handshake failed. Check the proxy settings of your computer or software upgrades.") );
		labelError->setText(msg.join(QStringLiteral("<br />")));
	}
}

void MobileProgress::stop()
{
	statusTimer->stop();
#ifdef Q_OS_WIN
	taskbar->progress()->stop();
	taskbar->progress()->hide();
#endif
}
