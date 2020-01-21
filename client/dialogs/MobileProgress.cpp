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
#include "ui_MobileProgress.h"

#include "Styles.h"

#include <common/Common.h>
#include <common/Configuration.h>

#include <digidocpp/crypto/X509Cert.h>

#include <QtCore/QDir>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QLoggingCategory>
#include <QtCore/QTimeLine>
#include <QtCore/QUuid>
#include <QtCore/QSettings>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#ifdef Q_OS_WIN
#include <QtWinExtras/QWinTaskbarButton>
#include <QtWinExtras/QWinTaskbarProgress>
#endif

Q_LOGGING_CATEGORY(MIDLog,"RIA.MID")

using namespace digidoc;

class MobileProgress::Private: public QDialog, public Ui::MobileProgress
{
	Q_OBJECT
public:
	QString URL() { return UUID.isNull() ? PROXYURL : SKURL; }
	using QDialog::QDialog;
	QTimeLine *statusTimer = nullptr;
	QNetworkAccessManager *manager = nullptr;
	QNetworkRequest req;
	QString ssid, cell, sessionID;
	std::vector<unsigned char> signature;
	X509Cert cert;
	QEventLoop l;
#ifdef CONFIG_URL
	QString PROXYURL = Configuration::instance().object().value(QStringLiteral("MID-PROXY-URL")).toString(QStringLiteral(MOBILEID_URL));
	QString SKURL = Configuration::instance().object().value(QStringLiteral("MID-SK-URL")).toString(QStringLiteral(MOBILEID_URL));
#else
	QString PROXYURL = QSettings().value(QStringLiteral("MID-PROXY-URL"), QStringLiteral(MOBILEID_URL)).toString();
	QString SKURL = QSettings().value(QStringLiteral("MID-SK-URL"), QStringLiteral(MOBILEID_URL)).toString();
#endif
	QString NAME = QSettings().value(QStringLiteral("MIDNAME"), QStringLiteral("RIA DigiDoc")).toString();
	QUuid UUID = QSettings().value(QStringLiteral("MIDUUID")).toUuid();
#ifdef Q_OS_WIN
	QWinTaskbarButton *taskbar = nullptr;
#endif
};

MobileProgress::MobileProgress(QWidget *parent)
	: d(new Private(parent))
{
	const_cast<QLoggingCategory&>(MIDLog()).setEnabled(QtDebugMsg,
		QFile::exists(QStringLiteral("%1/%2.log").arg(QDir::tempPath(), qApp->applicationName())));
	d->setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint);
	d->setupUi(d);
	d->code->setBuddy(d->signProgressBar);
	d->code->setFont(Styles::font(Styles::Regular, 20, QFont::DemiBold));
	d->labelError->setFont(Styles::font(Styles::Regular, 14));
	d->signProgressBar->setFont(d->labelError->font());
	d->cancel->setFont(Styles::font(Styles::Condensed, 14));
	QObject::connect(d->cancel, &QPushButton::clicked, d, &QDialog::reject);
	QObject::connect(d->cancel, &QPushButton::clicked,  [=] { d->l.exit(QDialog::Rejected); });

	d->statusTimer = new QTimeLine(d->signProgressBar->maximum() * 1000, d);
	d->statusTimer->setCurveShape(QTimeLine::LinearCurve);
	d->statusTimer->setFrameRange(d->signProgressBar->minimum(), d->signProgressBar->maximum());
	QObject::connect(d->statusTimer, &QTimeLine::frameChanged, d->signProgressBar, &QProgressBar::setValue);
	QObject::connect(d->statusTimer, &QTimeLine::finished, d, &QDialog::reject);
#ifdef Q_OS_WIN
	d->taskbar = new QWinTaskbarButton(d);
	d->taskbar->setWindow(parent->windowHandle());
	d->taskbar->progress()->setRange(d->signProgressBar->minimum(), d->signProgressBar->maximum());
	QObject::connect(d->statusTimer, &QTimeLine::frameChanged, d->taskbar->progress(), &QWinTaskbarProgress::setValue);
#endif

	QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();
	QList<QSslCertificate> trusted;
#ifdef CONFIG_URL
	ssl.setCaCertificates({});
	for(const QJsonValue cert: Configuration::instance().object().value(QStringLiteral("CERT-BUNDLE")).toArray())
		trusted << QSslCertificate(QByteArray::fromBase64(cert.toString().toLatin1()), QSsl::Der);
#endif
	d->req.setSslConfiguration(ssl);
	d->req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	d->req.setRawHeader("User-Agent", QStringLiteral("%1/%2 (%3)")
		.arg(qApp->applicationName(), qApp->applicationVersion(), Common::applicationOs()).toUtf8());
	d->manager = new QNetworkAccessManager(d);
	QObject::connect(d->manager, &QNetworkAccessManager::sslErrors, [=](QNetworkReply *reply, const QList<QSslError> &err) {
		QList<QSslError> ignore;
		for(const QSslError &e: err)
		{
			switch(e.error())
			{
			case QSslError::UnableToGetLocalIssuerCertificate:
			case QSslError::CertificateUntrusted:
			case QSslError::SelfSignedCertificateInChain:
				if(trusted.contains(reply->sslConfiguration().peerCertificate())) {
					ignore << e;
					break;
				}
			default:
				qCWarning(MIDLog) << "SSL Error:" << e.error() << e.certificate().subjectInfo(QSslCertificate::CommonName);
				break;
			}
		}
		reply->ignoreSslErrors(ignore);
	});
	QObject::connect(d->manager, &QNetworkAccessManager::finished, [=](QNetworkReply *reply) {
		QScopedPointer<QNetworkReply,QScopedPointerDeleteLater> scope(reply);
		auto returnError = [=](const QString &err) {
			qCWarning(MIDLog) << err;
			d->labelError->setText(err);
			d->code->hide();
			d->signProgressBar->hide();
			d->show();
			stop();
		};

		switch(reply->error())
		{
		case QNetworkReply::NoError:
			break;
		case QNetworkReply::ContentNotFoundError:
			returnError(d->sessionID.isEmpty() ? tr("Account not found") : tr("Session not found"));
			return;
		case QNetworkReply::HostNotFoundError:
			returnError(tr("Connecting to SK server failed! Please check your internet connection."));
			return;
		case QNetworkReply::ConnectionRefusedError:
			returnError(tr("%1 service has encountered technical errors. Please try again later.").arg(tr("Mobile-ID")));
			return;
		case QNetworkReply::SslHandshakeFailedError:
			returnError(tr("SSL handshake failed. Check the proxy settings of your computer or software upgrades."));
			return;
		default:
			qCWarning(MIDLog) << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() << "Error :" << reply->error();
			if(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 429)
				returnError(tr("The limit for digital signatures per month has been reached for this IP address. <a href=\"https://www.id.ee/index.php?id=39023\">Additional information</a>"));
			else if(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 400)
				break;
			else
				returnError(tr("Failed to send request. ") + reply->errorString());
			return;
		}
		static const QStringList contentType{"application/json", "application/json;charset=UTF-8"};
		if(!contentType.contains(reply->header(QNetworkRequest::ContentTypeHeader).toString()))
		{
			returnError(tr("Invalid content type header ") + reply->header(QNetworkRequest::ContentTypeHeader).toString());
			return;
		}

		QByteArray data = reply->readAll();
		qCDebug(MIDLog).noquote() << data;
		QJsonObject result = QJsonDocument::fromJson(data, nullptr).object();
		if(result.isEmpty())
		{
			returnError(tr("Failed to parse JSON content"));
			return;
		}

		if(result.contains("error"))
		{
			QString error =result["error"].toString();
			if(error == QStringLiteral("phoneNumber must contain of + and numbers(8-30)"))
				returnError(tr("Please include correct country code."));
			else
				returnError(tr(error.toUtf8().constData()));
			return;
		}
		if(result.contains(QStringLiteral("cert")))
		{
			try {
				QByteArray b64 = QByteArray::fromBase64(result.value(QStringLiteral("cert")).toString().toUtf8());
				d->cert = X509Cert((const unsigned char*)b64.constData(), size_t(b64.size()), X509Cert::Der);
				d->l.exit(QDialog::Accepted);
			} catch(const Exception &e) {
				returnError(tr("Failed to parse certificate: ") + QString::fromStdString(e.msg()));
			}
			return;
		}
		if(result.contains(QStringLiteral("sessionID")))
			d->sessionID = result.value(QStringLiteral("sessionID")).toString();
		else if(result.value(QStringLiteral("state")).toString() != QStringLiteral("RUNNING"))
		{
			QString endResult = result.value(QStringLiteral("result")).toString();
			if(endResult == QStringLiteral("OK"))
			{
				QByteArray b64 = QByteArray::fromBase64(
					result.value(QStringLiteral("signature")).toObject().value(QStringLiteral("value")).toString().toUtf8());
				d->signature.assign(b64.cbegin(), b64.cend());
				d->l.exit(QDialog::Accepted);
			}
			else if(endResult == QStringLiteral("USER_CANCELLED") || endResult == QStringLiteral("TIMEOUT"))
				d->l.exit(QDialog::Rejected);
			else if(endResult == QStringLiteral("NOT_MID_CLIENT") || endResult == QStringLiteral("NOT_FOUND") || endResult == QStringLiteral("NOT_ACTIVE"))
				returnError(tr("User is not a Mobile-ID client"));
			else if(endResult == QStringLiteral("PHONE_ABSENT"))
				returnError(tr("Phone absent"));
			else if(endResult == QStringLiteral("DELIVERY_ERROR"))
				returnError(tr("Request sending error"));
			else if(endResult == QStringLiteral("SIM_ERROR"))
				returnError(tr("SIM error"));
			else
				returnError(tr("Service result:") + endResult);
			return;
		}
		d->req.setUrl(QUrl(QStringLiteral("%1/signature/session/%2?timeoutMs=10000").arg(d->URL(), d->sessionID)));
		qCDebug(MIDLog).noquote() << d->req.url();
		d->manager->get(d->req);
	});
}

MobileProgress::~MobileProgress()
{
	d->deleteLater();
}

X509Cert MobileProgress::cert() const
{
	return d->cert;
}

bool MobileProgress::init(const QString &ssid, const QString &cell)
{
	d->ssid = ssid;
	d->cell = '+' + cell;
	d->labelError->setText(tr("Signing in process"));
	d->sessionID.clear();
	QByteArray data = QJsonDocument(QJsonObject::fromVariantHash(QVariantHash{
		{"relyingPartyUUID", d->UUID
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
			.toString(QUuid::WithoutBraces)
#else
			.toString().remove('{').remove('}')
#endif
		},
		{"relyingPartyName", d->NAME},
		{"nationalIdentityNumber", d->ssid},
		{"phoneNumber", d->cell},
	})).toJson();
	d->req.setUrl(QUrl(QStringLiteral("%1/certificate").arg(d->URL())));
	qCDebug(MIDLog).noquote() << d->req.url() << data;
	d->manager->post(d->req, data);
	return d->l.exec() == QDialog::Accepted;
}

std::vector<unsigned char> MobileProgress::sign(const std::string &method, const std::vector<unsigned char> &digest) const
{
	d->statusTimer->stop();
	d->sessionID.clear();

	QString digestMethod;
	if(method == "http://www.w3.org/2001/04/xmldsig-more#rsa-sha256" ||
		method == "http://www.w3.org/2001/04/xmldsig-more#ecdsa-sha256")
		digestMethod = QStringLiteral("SHA256");
	else if(method == "http://www.w3.org/2001/04/xmldsig-more#rsa-sha384" ||
			method == "http://www.w3.org/2001/04/xmldsig-more#ecdsa-sha384")
		digestMethod = QStringLiteral("SHA384");
	else if(method == "http://www.w3.org/2001/04/xmldsig-more#rsa-sha512" ||
			method == "http://www.w3.org/2001/04/xmldsig-more#ecdsa-sha512")
		digestMethod = QStringLiteral("SHA512");
	else
		throw Exception(__FILE__, __LINE__, "Unsupported digest method");

	d->code->setText(tr("Make sure control code matches with one in phone screen\n"
		"and enter Mobile-ID PIN2-code.\nControl code: %1")
		.arg((digest.front() >> 2) << 7 | (digest.back() & 0x7F), 4, 10, QChar('0')));

	QHash<QString,QString> lang;
	lang[QStringLiteral("et")] = QStringLiteral("EST");
	lang[QStringLiteral("en")] = QStringLiteral("ENG");
	lang[QStringLiteral("ru")] = QStringLiteral("RUS");

	QByteArray data = QJsonDocument(QJsonObject::fromVariantHash({
		{"relyingPartyUUID", d->UUID
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
			.toString(QUuid::WithoutBraces)
#else
			.toString().remove('{').remove('}')
#endif
		},
		{"relyingPartyName", d->NAME},
		{"nationalIdentityNumber", d->ssid},
		{"phoneNumber", d->cell},
		{"hash", QByteArray::fromRawData((const char*)digest.data(), int(digest.size())).toBase64()},
		{"hashType", digestMethod},
		{"language", lang.value(Common::language(), QStringLiteral("EST"))},
		{"displayText", "Sign document"},
		{"displayTextFormat", "GSM-7"}
	})).toJson();
	d->req.setUrl(QUrl(QStringLiteral("%1/signature").arg(d->URL())));
	qCDebug(MIDLog).noquote() << d->req.url() << data;
	d->manager->post(d->req, data);
	d->statusTimer->start();
	d->adjustSize();
	d->show();
	switch(d->l.exec())
	{
	case QDialog::Accepted: return d->signature;
	case QDialog::Rejected:
	{
		Exception e(__FILE__, __LINE__, "Signing canceled");
		e.setCode(Exception::PINCanceled);
		throw e;
	}
	default:
		throw Exception(__FILE__, __LINE__, "Failed to sign container");
	}
}

void MobileProgress::stop()
{
	d->statusTimer->stop();
#ifdef Q_OS_WIN
	d->taskbar->progress()->stop();
	d->taskbar->progress()->hide();
#endif
}

#include "MobileProgress.moc"
