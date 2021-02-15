/*
 * QDigiDocClient
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

#include "SmartIDProgress.h"
#include "ui_MobileProgress.h"

#include "Styles.h"
#include "WarningDialog.h"

#include <common/Common.h>
#include <common/Configuration.h>

#include <digidocpp/crypto/X509Cert.h>

#include <QtCore/QCryptographicHash>
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

Q_LOGGING_CATEGORY(SIDLog,"RIA.SmartID")

using namespace digidoc;

class SmartIDProgress::Private: public QDialog, public Ui::MobileProgress
{
	Q_OBJECT
public:
	QString URL() { return !UUID.isNull() && useCustomUUID ? SKURL : PROXYURL; }
	using QDialog::QDialog;
	void reject() override { l.exit(QDialog::Rejected); }
	QTimeLine *statusTimer = nullptr;
	QNetworkAccessManager *manager = nullptr;
	QNetworkRequest req;
	QString documentNumber, sessionID;
	X509Cert cert;
	std::vector<unsigned char> signature;
	QEventLoop l;
#ifdef CONFIG_URL
	QString PROXYURL = Configuration::instance().object().value(QStringLiteral("SID-PROXY-URL")).toString(QStringLiteral(SMARTID_URL));
	QString SKURL = Configuration::instance().object().value(QStringLiteral("SID-SK-URL")).toString(QStringLiteral(SMARTID_URL));
#else
	QString PROXYURL = QSettings().value(QStringLiteral("SID-PROXY-URL"), QStringLiteral(SMARTID_URL)).toString();
	QString SKURL = QSettings().value(QStringLiteral("SID-SK-URL"), QStringLiteral(SMARTID_URL)).toString();
#endif
	QString NAME = QSettings().value(QStringLiteral("SIDNAME"), QStringLiteral("RIA DigiDoc")).toString();
	bool useCustomUUID = QSettings().value(QStringLiteral("SIDUUID-CUSTOM"), QSettings().contains(QStringLiteral("SIDUUID"))).toBool();
	QString UUID = useCustomUUID ? QSettings().value(QStringLiteral("SIDUUID")).toString() : QString();
#ifdef Q_OS_WIN
	QWinTaskbarButton *taskbar = nullptr;
#endif
};



SmartIDProgress::SmartIDProgress(QWidget *parent)
	: d(new Private(parent))
{
	const_cast<QLoggingCategory&>(SIDLog()).setEnabled(QtDebugMsg,
		true || QFile::exists(QStringLiteral("%1/%2.log").arg(QDir::tempPath(), qApp->applicationName())));
	d->setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint);
	d->setupUi(d);
	d->move(parent->geometry().center() - d->geometry().center());
	d->signProgressBar->setMaximum(100);
	d->code->setBuddy(d->signProgressBar);
	d->code->setFont(Styles::font(Styles::Regular, 48));
	d->info->setFont(Styles::font(Styles::Regular, 14));
	d->controlCode->setFont(Styles::font(Styles::Regular, 14));
	d->signProgressBar->setFont(d->info->font());
	d->cancel->setFont(Styles::font(Styles::Condensed, 14));
	QObject::connect(d->cancel, &QPushButton::clicked, d, &QDialog::reject);

	d->statusTimer = new QTimeLine(d->signProgressBar->maximum() * 1000, d);
	d->statusTimer->setCurveShape(QTimeLine::LinearCurve);
	d->statusTimer->setFrameRange(d->signProgressBar->minimum(), d->signProgressBar->maximum());
	QObject::connect(d->statusTimer, &QTimeLine::frameChanged, d->signProgressBar, &QProgressBar::setValue);
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
	QObject::connect(d->manager, &QNetworkAccessManager::sslErrors, d->manager, [=](QNetworkReply *reply, const QList<QSslError> &err) {
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
				Q_FALLTHROUGH();
			default:
				qCWarning(SIDLog) << "SSL Error:" << e.error() << e.certificate().subjectInfo(QSslCertificate::CommonName);
				break;
			}
		}
		reply->ignoreSslErrors(ignore);
	});
	QNetworkAccessManager::connect(d->manager, &QNetworkAccessManager::finished, d, [&](QNetworkReply *reply){
		QScopedPointer<QNetworkReply,QScopedPointerDeleteLater> scope(reply);
		auto returnError = [=](const QString &err, const QString &details = {}) {
			qCWarning(SIDLog) << err;
			d->hide();
			WarningDialog dlg(err, details, d->parentWidget());
			QObject::connect(&dlg, &WarningDialog::finished, &d->l, &QEventLoop::exit);
			dlg.exec();
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
			returnError(tr("%1 service has encountered technical errors. Please try again later.").arg(tr("Smart-ID")));
			return;
		case QNetworkReply::SslHandshakeFailedError:
			returnError(tr("SSL handshake failed. Check the proxy settings of your computer or software upgrades."));
			return;
		case QNetworkReply::TimeoutError:
		case QNetworkReply::UnknownNetworkError:
			returnError(tr("Failed to connect with service server. Please check your network settings or try again later."));
			return;
		case QNetworkReply::ProtocolInvalidOperationError:
			qCWarning(SIDLog) << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
			returnError(reply->readAll());
			return;
		case QNetworkReply::AuthenticationRequiredError:
			returnError(tr("Failed to send request. Check your %1 service access settings.").arg(tr("Smart-ID")));
			return;
		default:
			qCWarning(SIDLog) << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() << "Error :" << reply->error();
			switch (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt())
			{
			case 403:
				returnError(tr("Failed to sign container. Check your %1 service access settings. "
					"<a href=\"https://www.id.ee/en/article/for-organisations-that-sign-large-quantities-of-documents-using-digidoc4-client/\">Additional information</a>").arg(tr("Smart-ID")));
				return;
			case 429:
				returnError(tr("The limit for %1 digital signatures per month has been reached for this IP address. "
					"<a href=\"https://www.id.ee/en/article/for-organisations-that-sign-large-quantities-of-documents-using-digidoc4-client/\">Additional information</a>").arg(tr("Smart-ID")));
				return;
			case 471:
				returnError(tr("Your Smart-ID certificate level must be qualified to sign documents in DigiDoc4 Client."));
				return;
			case 480:
				returnError(tr("Your signing software needs an upgrade. Please update your ID software, which you can get from "
					"<a href=\"https://www.id.ee/en/\">www.id.ee</a>. Additional info is available ID-helpline (+372) 666 8888."));
				return;
			case 580:
				returnError(tr("%1 service has encountered technical errors. Please try again later.").arg(QStringLiteral("Smart-ID")));
				return;
			default:
				returnError(tr("Failed to send request. %1 service has encountered technical errors. Please try again later.").arg(QStringLiteral("Smart-ID")), reply->errorString());
				return;
			}
		}
		static const QStringList contentType{"application/json", "application/json;charset=UTF-8"};
		if(!contentType.contains(reply->header(QNetworkRequest::ContentTypeHeader).toString()))
		{
			returnError(tr("Invalid content type header ") + reply->header(QNetworkRequest::ContentTypeHeader).toString());
			return;
		}

		QByteArray data = reply->readAll();
		reply->deleteLater();
		qCDebug(SIDLog).noquote() << data;
		QJsonObject result = QJsonDocument::fromJson(data, nullptr).object();
		if(result.isEmpty())
		{
			returnError(tr("Failed to parse JSON content"));
			return;
		}
		if(d->sessionID.isEmpty())
			d->sessionID = result.value(QStringLiteral("sessionID")).toString();
		else if(result.value(QStringLiteral("state")).toString() != QStringLiteral("RUNNING"))
		{
			QString endResult = result.value(QStringLiteral("result")).toObject().value(QStringLiteral("endResult")).toString();
			if(endResult == QStringLiteral("USER_REFUSED"))
				returnError(tr("User denied or cancelled"));
			else if(endResult == QStringLiteral("TIMEOUT"))
				returnError(tr("Your Smart-ID transaction has expired. Please try again."));
			else if(endResult == QStringLiteral("WRONG_VC"))
				returnError(tr("Error: an incorrect control code was chosen"));
			else if(endResult == QStringLiteral("DOCUMENT_UNUSABLE"))
				returnError(tr("Your Smart-ID transaction has failed. Please check your Smart-ID application or contact Smart-ID customer support."));
			else if(endResult != QStringLiteral("OK"))
				returnError(tr("Service result: ") + endResult);
			else if(d->documentNumber.isEmpty())
				d->documentNumber = result.value(QStringLiteral("result")).toObject().value(QStringLiteral("documentNumber")).toString();
			if(result.contains(QStringLiteral("signature")))
			{
				QByteArray b64 = QByteArray::fromBase64(
					result.value(QStringLiteral("signature")).toObject().value(QStringLiteral("value")).toString().toUtf8());
				d->signature.assign(b64.cbegin(), b64.cend());
				d->l.exit(QDialog::Accepted);
			}
			else if(result.contains(QStringLiteral("cert")))
			{
				try {
					QByteArray b64 = QByteArray::fromBase64(
						result.value(QStringLiteral("cert")).toObject().value(QStringLiteral("value")).toString().toUtf8());
					d->cert = X509Cert((const unsigned char*)b64.constData(), size_t(b64.size()), X509Cert::Der);
					d->l.exit(QDialog::Accepted);
				} catch(const Exception &e) {
					returnError(tr("Failed to parse certificate: ") + QString::fromStdString(e.msg()));
				}
			}
			return;
		}
		d->req.setUrl(QUrl(QStringLiteral("%1/session/%2?timeoutMs=10000").arg(d->URL(), d->sessionID)));
		qCDebug(SIDLog).noquote() << d->req.url();
		d->manager->get(d->req);
	});
}

SmartIDProgress::~SmartIDProgress()
{
	d->deleteLater();
}

X509Cert SmartIDProgress::cert() const
{
	return d->cert;
}

bool SmartIDProgress::init(const QString &country, const QString &idCode)
{
	if(!d->UUID.isEmpty() && QUuid(d->UUID).isNull())
	{
		WarningDialog(tr("Failed to send request. Check your %1 service access settings.").arg(tr("Smart-ID")), {}, d->parentWidget()).exec();
		return false;
	}
	d->sessionID.clear();
	QByteArray data = QJsonDocument({
		{"relyingPartyUUID", d->UUID.isEmpty() ? QStringLiteral("00000000-0000-0000-0000-000000000000") : d->UUID},
		{"relyingPartyName", d->NAME},
		{"certificateLevel", "QUALIFIED"},
		{"nonce", QUuid::createUuid().toString().remove('-').mid(1, 30)}
	}).toJson();
	d->req.setUrl(QUrl(QStringLiteral("%1/certificatechoice/pno/%2/%3").arg(d->URL(), country, idCode)));
	qCDebug(SIDLog).noquote() << d->req.url() << data;
	d->manager->post(d->req, data);
	d->info->setText(tr("Open the Smart-ID application on your smart device and confirm device for signing."));
	d->code->setAccessibleName(d->info->text());
	d->statusTimer->start();
	d->adjustSize();
	d->show();
	return d->l.exec() == QDialog::Accepted;
}

std::vector<unsigned char> SmartIDProgress::sign(const std::string &method, const std::vector<unsigned char> &digest) const
{
	d->statusTimer->stop();
	d->sessionID.clear();

	QString digestMethod;
	if(method == "http://www.w3.org/2001/04/xmldsig-more#rsa-sha256")
		digestMethod = QStringLiteral("SHA256");
	else if(method == "http://www.w3.org/2001/04/xmldsig-more#rsa-sha384")
		digestMethod = QStringLiteral("SHA384");
	else if(method == "http://www.w3.org/2001/04/xmldsig-more#rsa-sha512")
		digestMethod = QStringLiteral("SHA512");
	else
		throw Exception(__FILE__, __LINE__, "Unsupported digest method");

	QByteArray codeDiest = QCryptographicHash::hash(QByteArray::fromRawData((const char*)digest.data(), int(digest.size())), QCryptographicHash::Sha256);
	uint code = codeDiest.right(2).toHex().toUInt(nullptr, 16) % 10000;
	d->code->setText(QStringLiteral("%1").arg(code, 4, 10, QChar('0')));
	d->info->setText(tr("Make sure control code matches with one in phone screen\n"
		"and enter Smart-ID PIN2-code."));
	d->code->setAccessibleName(QStringLiteral("%1 %2. %3").arg(d->controlCode->text(), d->code->text(), d->info->text()));

	QByteArray data = QJsonDocument({
		{"relyingPartyUUID", (d->UUID.isEmpty() ? QStringLiteral("00000000-0000-0000-0000-000000000000") : d->UUID)},
		{"relyingPartyName", d->NAME},
		{"certificateLevel", "QUALIFIED"},
		{"hash", QString(QByteArray::fromRawData((const char*)digest.data(), int(digest.size())).toBase64())},
		{"hashType", digestMethod},
		{"requestProperties", QJsonObject{{"vcChoice", true}}},
		{"displayText", tr("Sign document", "Do not translate to RUS (IB-6416)")}
	}).toJson();
	d->req.setUrl(QUrl(QStringLiteral("%1/signature/document/%2").arg(d->URL(), d->documentNumber)));
	qCDebug(SIDLog).noquote() << d->req.url() << data;
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

void SmartIDProgress::stop()
{
	d->statusTimer->stop();
#ifdef Q_OS_WIN
	d->taskbar->progress()->stop();
	d->taskbar->progress()->hide();
#endif
}

#include "SmartIDProgress.moc"
