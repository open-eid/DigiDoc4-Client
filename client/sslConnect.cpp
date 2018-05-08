/*
 * QEstEidUtil
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
 
#include "sslConnect.h"

#include <common/Common.h>
#include <common/Configuration.h>
#include <common/SOAPDocument.h>
#include <common/Settings.h>

#include <QtCore/QJsonObject>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QProgressDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QHBoxLayout>

class SSLConnect::Private
{
public:
	QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();
	QString errorString;
};

SSLConnect::SSLConnect(QObject *parent)
	: QNetworkAccessManager(parent)
	, d(new Private)
{
	connect(this, &QNetworkAccessManager::sslErrors, this, [=](QNetworkReply *reply, const QList<QSslError> &errors){
		QList<QSslError> ignore;
		for(const QSslError &error: errors)
		{
			switch(error.error())
			{
			case QSslError::UnableToGetLocalIssuerCertificate:
			case QSslError::CertificateUntrusted:
				//if(trusted.contains(reply->sslConfiguration().peerCertificate()))
					ignore << error;
				break;
			default: break;
			}
		}
		reply->ignoreSslErrors(ignore);
	});
}

SSLConnect::~SSLConnect()
{
	delete d;
}

QByteArray SSLConnect::getUrl(RequestType type, const QString &value)
{
	QJsonObject obj = Configuration::instance().object();
	QString label;
	QByteArray contentType;
	QNetworkRequest req;
	switch(type)
	{
	case MobileInfo:
	{
		label = tr("Loading Mobile info");
		req = QNetworkRequest(
			obj.value(QLatin1String("MID-URL")).toString(QStringLiteral("https://id.sk.ee/MIDInfoWS/")));
		req.setRawHeader( "Content-Type", "text/xml" );
		req.setRawHeader( "SOAPAction", QByteArray() );
		req.setRawHeader( "Connection", "close" );
		contentType = "text/xml";
		break;
	}
	case EmailInfo:
		label = tr("Loading Email info");
		req = QNetworkRequest(
			obj.value(QLatin1String("EMAIL-REDIRECT-URL")).toString(QStringLiteral("https://sisene.www.eesti.ee/idportaal/postisysteem.naita_suunamised")));
		contentType = "application/xml";
		break;
	case ActivateEmails:
		label = tr("Loading Email info");
		req = QNetworkRequest(
			obj.value(QLatin1String("EMAIL-ACTIVATE-URL")).toString(QStringLiteral("https://sisene.www.eesti.ee/idportaal/postisysteem.lisa_suunamine?=%1")).arg(value));
		contentType = "application/xml";
		break;
	case PictureInfo:
		label = tr("Downloading picture");
		req = QNetworkRequest(
			obj.value(QLatin1String("PICTURE-URL")).toString(QStringLiteral("https://sisene.www.eesti.ee/idportaal/portaal.idpilt")));
		contentType = "image/jpeg";
		break;
	default: return QByteArray();
	}

//	Does not align it to center on Unix
	QFrame popup (qApp->activeWindow(), Qt::Tool | Qt::Window | Qt::FramelessWindowHint);
	showPopup(popup, label);

	req.setSslConfiguration(d->ssl);
	req.setRawHeader("User-Agent", QString(QStringLiteral("%1/%2 (%3)"))
		.arg(qApp->applicationName(), qApp->applicationVersion(), Common::applicationOs()).toUtf8());
	QNetworkReply *reply = nullptr;
	if (type == MobileInfo)
	{
		req.setHeader(QNetworkRequest::ContentTypeHeader, "text/xml");
		SOAPDocument s("GetMIDTokens", "urn:GetMIDTokens");
		s.writeEndDocument();
		reply = post(req, s.document());
	}
	else
	{
		req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
		reply = get(req);
	}

	QEventLoop e;
	connect(reply, &QNetworkReply::finished, &e, &QEventLoop::quit);
	e.exec();

	if(reply->error() != QNetworkReply::NoError)
	{
		d->errorString = reply->errorString();
		return QByteArray();
	}
	if(!reply->header(QNetworkRequest::ContentTypeHeader).toByteArray().contains(contentType))
	{
		d->errorString = tr("Invalid Content-Type");
		return QByteArray();
	}
	QByteArray result = reply->readAll();
	reply->deleteLater();
	return result;
}

QString SSLConnect::errorString() const { return d->errorString; }

void SSLConnect::setToken(const QSslCertificate &cert, const QSslKey &key)
{
	d->ssl.setPrivateKey(key);
	d->ssl.setLocalCertificate(cert);
}

void SSLConnect::showPopup( QFrame &popup, const QString &labelText )
{
	popup.resize( 328, 179 );
	popup.setStyleSheet( "background-color: #e8e8e8; border: solid #5e5e5e; border-width: 1px 1px 1px 1px;" );

	QLabel *lbl = new QLabel( &popup );
	lbl->setStyleSheet( QString( "font-size: 24px; color: #53c964; border: none;" ) );
	lbl->setAlignment( Qt::AlignHCenter | Qt::AlignVCenter );
	lbl->setText( labelText );
	lbl->show();

	QHBoxLayout* layout=new QHBoxLayout ( &popup );
	layout->addWidget( lbl );
	popup.setLayout( layout );

	popup.show();
}
