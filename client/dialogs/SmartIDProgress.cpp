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

#include "Application.h"
#include "CheckConnection.h"
#include "Settings.h"
#include "Styles.h"
#include "Utils.h"
#include "dialogs/WarningDialog.h"

#include <digidocpp/crypto/X509Cert.h>

#include <QtCore/QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QLoggingCategory>
#include <QtCore/QTimeLine>
#include <QtCore/QUuid>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>

#include <chrono>

Q_LOGGING_CATEGORY(SIDLog,"RIA.SmartID")

using namespace digidoc;

class SmartIDProgress::Private final: public QDialog, public Ui::MobileProgress
{
	Q_OBJECT
public:
	using QDialog::QDialog;
	void reject() final { l.exit(QDialog::Rejected); }
	void setVisible(bool visible) final {
		if(visible && !hider) hider = std::make_unique<WaitDialogHider>();
		QDialog::setVisible(visible);
		if(!visible && hider) hider.reset();
	}
	QTimer *timer{};
	QTimeLine *statusTimer{};
	QNetworkAccessManager *manager {};
	QNetworkRequest req;
	QString documentNumber, sessionID, fileName;
	X509Cert cert;
	std::vector<unsigned char> signature;
	QEventLoop l;
	bool useCustomUUID = Settings::SID_UUID_CUSTOM;
	QString UUID = useCustomUUID ? Settings::SID_UUID : QString();
	QString NAME = Settings::SID_NAME;
	QString URL = !UUID.isNull() && useCustomUUID ? Settings::SID_SK_URL : Settings::SID_PROXY_URL;
	std::unique_ptr<WaitDialogHider> hider;
};



SmartIDProgress::SmartIDProgress(QWidget *parent)
	: d(new Private(parent))
{
	const_cast<QLoggingCategory&>(SIDLog()).setEnabled(QtDebugMsg,
		QFile::exists(QStringLiteral("%1/%2.log").arg(QDir::tempPath(), QApplication::applicationName())));
	d->setWindowFlags(Qt::Dialog|Qt::CustomizeWindowHint);
	d->setupUi(d);
	d->move(parent->geometry().center() - d->geometry().center());
	d->signProgressBar->setMaximum(100);
	d->code->setBuddy(d->signProgressBar);
	d->code->setFont(Styles::font(Styles::Regular, 48));
	d->info->setFont(Styles::font(Styles::Regular, 14));
	d->controlCode->setFont(d->info->font());
	d->signProgressBar->setFont(d->info->font());
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
const auto styleSheet = R"(QProgressBar {
background-color: #d3d3d3;;
border-style: solid;
border-radius: 3px;
min-height: 6px;
max-height: 6px;
margin: 15px 5px;
}
QProgressBar::chunk {
border-style: solid;
border-radius: 3px;
background-color: #007aff;
})";
	d->signProgressBar->setStyleSheet(styleSheet);
#endif
	d->cancel->setFont(Styles::font(Styles::Condensed, 14));
	QObject::connect(d->cancel, &QPushButton::clicked, d, &QDialog::reject);

	d->statusTimer = new QTimeLine(d->signProgressBar->maximum() * 1000, d);
	d->statusTimer->setEasingCurve(QEasingCurve::Linear);
	d->statusTimer->setFrameRange(d->signProgressBar->minimum(), d->signProgressBar->maximum());
	QObject::connect(d->statusTimer, &QTimeLine::frameChanged, d->signProgressBar, &QProgressBar::setValue);

	d->req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	d->manager = CheckConnection::setupNAM(d->req);
	d->manager->setParent(d);
	QNetworkAccessManager::connect(d->manager, &QNetworkAccessManager::finished, d, [&](QNetworkReply *reply){
		QScopedPointer<QNetworkReply,QScopedPointerDeleteLater> scope(reply);
		auto returnError = [=](const QString &err, const QString &details = {}) {
			qCWarning(SIDLog) << err;
			d->statusTimer->stop();
			delete d->timer;
			d->timer = nullptr;
			d->hide();
			auto *dlg = WarningDialog::show(d->parentWidget(), err, details);
			QObject::connect(dlg, &WarningDialog::finished, &d->l, &QEventLoop::exit);
		};

		switch(reply->error())
		{
		case QNetworkReply::NoError:
			break;
		case QNetworkReply::ContentNotFoundError:
			return returnError(tr("Failed to sign container. Your Smart-ID transaction has expired or user account not found."));
		case QNetworkReply::ConnectionRefusedError:
			return returnError(tr("%1 service has encountered technical errors. Please try again later.").arg(tr("Smart-ID")));
		case QNetworkReply::SslHandshakeFailedError:
			return returnError(tr("SSL handshake failed. Check the proxy settings of your computer or software upgrades."));
		case QNetworkReply::TimeoutError:
		case QNetworkReply::UnknownNetworkError:
			return returnError(tr("Failed to connect with service server. Please check your network settings or try again later."));
		case QNetworkReply::HostNotFoundError:
		case QNetworkReply::AuthenticationRequiredError:
			return returnError(tr("Failed to send request. Check your %1 service access settings.").arg(tr("Smart-ID")));
		default:
			const auto httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
			qCWarning(SIDLog) << httpStatusCode << "Error :" << reply->error();
			switch (httpStatusCode)
			{
			case 403:
				return returnError(tr("Failed to sign container. Check your %1 service access settings. "
					"<a href=\"https://www.id.ee/en/article/for-organisations-that-sign-large-quantities-of-documents-using-digidoc4-client/\">Additional information</a>").arg(tr("Smart-ID")));
			case 409:
				return returnError(tr("Failed to send request. The number of unsuccesful request from this IP address has been exceeded. Please try again later."));
			case 429:
				return returnError(tr("The limit for %1 digital signatures per month has been reached for this IP address. "
					"<a href=\"https://www.id.ee/en/article/for-organisations-that-sign-large-quantities-of-documents-using-digidoc4-client/\">Additional information</a>").arg(tr("Smart-ID")));
			case 471:
				return returnError(tr("Your Smart-ID certificate level must be qualified to sign documents in DigiDoc4 Client."));
			case 480:
				return returnError(tr("Your signing software needs an upgrade. Please update your ID software, which you can get from "
					"<a href=\"https://www.id.ee/en/\">www.id.ee</a>. Additional info is available ID-helpline (+372) 666 8888."));
			case 580:
				return returnError(tr("Failed to send request. A valid session is associated with this personal code. "
					"It is not possible to start a new signing before the current session expires. Please try again later."));
			default:
				return returnError(tr("Failed to send request. %1 service has encountered technical errors. Please try again later.").arg(QLatin1String("Smart-ID")), reply->errorString());
			}
		}
		if(!reply->header(QNetworkRequest::ContentTypeHeader).toString().startsWith(QLatin1String("application/json")))
			return returnError(tr("Invalid content type header ") + reply->header(QNetworkRequest::ContentTypeHeader).toString());

		QByteArray data = reply->readAll();
		reply->deleteLater();
		qCDebug(SIDLog).noquote() << data;
		QJsonObject result = QJsonDocument::fromJson(data, nullptr).object();
		if(result.isEmpty())
			return returnError(tr("Failed to parse JSON content"));
		if(d->sessionID.isEmpty())
			d->sessionID = result.value(QLatin1String("sessionID")).toString();
		else if(result.value(QLatin1String("state")).toString() != QLatin1String("RUNNING"))
		{
			QString endResult = result.value(QLatin1String("result")).toObject().value(QLatin1String("endResult")).toString();
			if(endResult == QLatin1String("USER_REFUSED") ||
				endResult == QLatin1String("USER_REFUSED_CERT_CHOICE") ||
				endResult == QLatin1String("USER_REFUSED_DISPLAYTEXTANDPIN") ||
				endResult == QLatin1String("USER_REFUSED_VC_CHOICE") ||
				endResult == QLatin1String("USER_REFUSED_CONFIRMATIONMESSAGE") ||
				endResult == QLatin1String("USER_REFUSED_CONFIRMATIONMESSAGE_WITH_VC_CHOICE"))
				returnError(tr("User denied or cancelled"));
			else if(endResult == QLatin1String("TIMEOUT"))
				returnError(tr("Failed to sign container. Your Smart-ID transaction has expired or user account not found."));
			else if(endResult == QLatin1String("REQUIRED_INTERACTION_NOT_SUPPORTED_BY_APP"))
				returnError(tr("Failed to sign container. You need to update your Smart-ID application to sign documents in DigiDoc4 Client."));
			else if(endResult == QLatin1String("WRONG_VC"))
				returnError(tr("Error: an incorrect control code was chosen"));
			else if(endResult == QLatin1String("DOCUMENT_UNUSABLE"))
				returnError(tr("Your Smart-ID transaction has failed. Please check your Smart-ID application or contact Smart-ID customer support."));
			else if(endResult != QLatin1String("OK"))
				returnError(tr("Service result: ") + endResult);
			else if(d->documentNumber.isEmpty())
				d->documentNumber = result.value(QLatin1String("result")).toObject().value(QLatin1String("documentNumber")).toString();
			if(result.contains(QLatin1String("signature")))
			{
				QByteArray b64 = QByteArray::fromBase64(
					result.value(QLatin1String("signature")).toObject().value(QLatin1String("value")).toString().toUtf8());
				d->signature.assign(b64.cbegin(), b64.cend());
				d->hide();
				d->l.exit(QDialog::Accepted);
			}
			else if(result.contains(QLatin1String("cert")))
			{
				try {
					QByteArray b64 = QByteArray::fromBase64(
						result.value(QLatin1String("cert")).toObject().value(QLatin1String("value")).toString().toUtf8());
					d->cert = X509Cert((const unsigned char*)b64.constData(), size_t(b64.size()), X509Cert::Der);
					d->hide();
					d->l.exit(QDialog::Accepted);
				} catch(const Exception &e) {
					returnError(tr("Failed to parse certificate: ") + QString::fromStdString(e.msg()));
				}
			}
			return;
		}
		d->req.setUrl(QUrl(QStringLiteral("%1/session/%2?timeoutMs=10000").arg(d->URL, d->sessionID)));
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

bool SmartIDProgress::init(const QString &country, const QString &idCode, const QString &fileName)
{
	if(!d->UUID.isEmpty() && QUuid(d->UUID).isNull())
	{
		WarningDialog::show(d->parentWidget(), tr("Failed to send request. Check your %1 service access settings.").arg(tr("Smart-ID")));
		return false;
	}
	QFileInfo info(fileName);
	QString displayFile = info.completeBaseName();
	if(displayFile.size() > 6)
		displayFile = displayFile.left(3) + QStringLiteral("...") + displayFile.right(3);
	displayFile += QStringLiteral(".") + info.suffix();
	d->fileName = displayFile;
	d->sessionID.clear();
	QByteArray data = QJsonDocument({
		{"relyingPartyUUID", d->UUID.isEmpty() ? QStringLiteral("00000000-0000-0000-0000-000000000000") : d->UUID},
		{"relyingPartyName", d->NAME},
		{"certificateLevel", "QUALIFIED"},
		{"nonce", QUuid::createUuid().toString().remove('-').mid(1, 30)}
	}).toJson();
	if (d->req.url().path().contains(QLatin1String("v1"), Qt::CaseInsensitive)) {
		d->req.setUrl(QUrl(QStringLiteral("%1/certificatechoice/pno/%2/%3").arg(d->URL, country, idCode)));
	} else {
		d->req.setUrl(QUrl(QStringLiteral("%1/certificatechoice/etsi/PNO%2-%3").arg(d->URL, country, idCode)));
	}
	qCDebug(SIDLog).noquote() << d->req.url() << data;
	d->manager->post(d->req, data);
	d->info->setText(tr("Open the Smart-ID application on your smart device and confirm device for signing."));
	d->code->setAccessibleName(d->info->text());
	d->statusTimer->start();
	d->adjustSize();
	d->timer = new QTimer(d);
	d->timer->setSingleShot(true);
	QObject::connect(d->timer, &QTimer::timeout, d, &SmartIDProgress::Private::show);
	using namespace std::chrono;
	d->timer->start(3s);
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

	QJsonObject req{
		{"relyingPartyUUID", (d->UUID.isEmpty() ? QStringLiteral("00000000-0000-0000-0000-000000000000") : d->UUID)},
		{"relyingPartyName", d->NAME},
		{"certificateLevel", "QUALIFIED"},
		{"hash", QString(QByteArray::fromRawData((const char*)digest.data(), int(digest.size())).toBase64())},
		{"hashType", digestMethod},
	};
	QString escape = tr("Sign document");
	if (d->req.url().path().contains(QLatin1String("v1"), Qt::CaseInsensitive)) {
		req[QLatin1String("requestProperties")] = QJsonObject{{"vcChoice", true}};
		req[QLatin1String("displayText")] = "%1";
	} else {
		req[QLatin1String("allowedInteractionsOrder")] = QJsonArray{QJsonObject{
			{"type", "confirmationMessageAndVerificationCodeChoice"},
			{"displayText200", "%1"}
		}};
		escape = QStringLiteral("%1 %2").arg(tr("Sign document"), d->fileName);
	}
	// Workaround SID proxy issues
	QByteArray data = QString::fromUtf8(QJsonDocument(req).toJson()).arg(escapeUnicode(escape)).toUtf8();
	d->req.setUrl(QUrl(QStringLiteral("%1/signature/document/%2").arg(d->URL, d->documentNumber)));
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

#include "SmartIDProgress.moc"
