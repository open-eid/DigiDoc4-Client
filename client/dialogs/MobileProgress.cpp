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

#include "Application.h"
#include "CheckConnection.h"
#include "Settings.h"
#include "Styles.h"
#include "Utils.h"
#include "dialogs/WarningDialog.h"

#include <digidocpp/crypto/X509Cert.h>

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

Q_LOGGING_CATEGORY(MIDLog,"RIA.MID")

using namespace digidoc;

class MobileProgress::Private final: public QDialog, public Ui::MobileProgress
{
	Q_OBJECT
public:
	using QDialog::QDialog;
	void reject() final { l.exit(QDialog::Rejected); }
	QTimeLine *statusTimer{};
	QNetworkAccessManager *manager {};
	QNetworkRequest req;
	QString ssid, cell, sessionID;
	std::vector<unsigned char> signature;
	X509Cert cert;
	QEventLoop l;
	bool useCustomUUID = Settings::MID_UUID_CUSTOM;
	QString NAME = Settings::MID_NAME;
	QString UUID = useCustomUUID ? Settings::MID_UUID : QString();
	QString URL = !UUID.isNull() && useCustomUUID ? Settings::MID_SK_URL : Settings::MID_PROXY_URL;
};

MobileProgress::MobileProgress(QWidget *parent)
	: d(new Private(parent))
{
	const_cast<QLoggingCategory&>(MIDLog()).setEnabled(QtDebugMsg,
		QFile::exists(QStringLiteral("%1/%2.log").arg(QDir::tempPath(), QApplication::applicationName())));
	d->setWindowFlags(Qt::Dialog|Qt::CustomizeWindowHint);
	d->setupUi(d);
	d->move(parent->geometry().center() - d->geometry().center());
	d->code->setBuddy(d->signProgressBar);
	d->code->setFont(Styles::font(Styles::Regular, 48));
	d->info->setFont(Styles::font(Styles::Regular, 14));
	d->controlCode->setFont(d->info->font());
	d->signProgressBar->setFont(d->info->font());
#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
const auto styleSheet = R"(QProgressBar {
background-color: #d3d3d3;
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
	QObject::connect(d->statusTimer, &QTimeLine::finished, d, &QDialog::reject);

	d->req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	d->manager = CheckConnection::setupNAM(d->req);
	d->manager->setParent(d);
	QObject::connect(d->manager, &QNetworkAccessManager::finished, d, [=](QNetworkReply *reply) {
		QScopedPointer<QNetworkReply,QScopedPointerDeleteLater> scope(reply);
		auto returnError = [=](const QString &err, const QString &details = {}) {
			qCWarning(MIDLog) << err;
			d->statusTimer->stop();
			d->hide();
			auto *dlg = WarningDialog::show(d->parentWidget(), err, details);
			QObject::connect(dlg, &WarningDialog::finished, &d->l, &QEventLoop::exit);
		};

		switch(reply->error())
		{
		case QNetworkReply::NoError:
			break;
		case QNetworkReply::ContentNotFoundError:
			return returnError(d->sessionID.isEmpty() ? tr("Account not found") : tr("Session not found"));
		case QNetworkReply::ConnectionRefusedError:
			return returnError(tr("%1 service has encountered technical errors. Please try again later.").arg(tr("Mobile-ID")));
		case QNetworkReply::SslHandshakeFailedError:
			return returnError(tr("SSL handshake failed. Check the proxy settings of your computer or software upgrades."));
		case QNetworkReply::TimeoutError:
		case QNetworkReply::UnknownNetworkError:
			return returnError(tr("Failed to connect with service server. Please check your network settings or try again later."));
		case QNetworkReply::HostNotFoundError:
		case QNetworkReply::AuthenticationRequiredError:
			return returnError(tr("Failed to send request. Check your %1 service access settings.").arg(tr("mobile-ID")));
		default:
		{
			const auto httpStatusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
			qCWarning(MIDLog) << httpStatusCode << "Error :" << reply->error();
			switch(httpStatusCode)
			{
			case 409:
				return returnError(tr("Failed to send request. The number of unsuccesful request from this IP address has been exceeded. Please try again later."));
			case 429:
				return returnError(tr("The limit for %1 digital signatures per month has been reached for this IP address. "
					"<a href=\"https://www.id.ee/en/article/for-organisations-that-sign-large-quantities-of-documents-using-digidoc4-client/\">Additional information</a>").arg(tr("mobile-ID")));
			case 580:
				return returnError(tr("Failed to send request. A valid session is associated with this personal code. "
					"It is not possible to start a new signing before the current session expires. Please try again later."));
			default:
				return returnError(tr("Failed to send request. %1 service has encountered technical errors. Please try again later.").arg(tr("Mobile-ID")), reply->errorString());
			}
		}
		} // switch (reply->error())
		if(!reply->header(QNetworkRequest::ContentTypeHeader).toString().startsWith(QLatin1String("application/json")))
			return returnError(tr("Invalid content type header ") + reply->header(QNetworkRequest::ContentTypeHeader).toString());

		QByteArray data = reply->readAll();
		qCDebug(MIDLog).noquote() << data;
		QJsonObject result = QJsonDocument::fromJson(data, nullptr).object();
		if(result.isEmpty())
			return returnError(tr("Failed to parse JSON content"));

		if(result.contains(QStringLiteral("error")))
		{
			QString error =result[QStringLiteral("error")].toString();
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
				d->hide();
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
				d->hide();
				d->l.exit(QDialog::Accepted);
			}
			else if(endResult == QStringLiteral("NOT_MID_CLIENT") || endResult == QStringLiteral("NOT_FOUND") || endResult == QStringLiteral("NOT_ACTIVE"))
				returnError(tr("User is not a mobile-ID client"));
			else if(endResult == QStringLiteral("USER_CANCELLED"))
				returnError(tr("User denied or cancelled"));
			else if(endResult == QStringLiteral("TIMEOUT"))
				returnError(tr("Your mobile-ID transaction has expired. Please try again."));
			else if(endResult == QStringLiteral("PHONE_ABSENT"))
				returnError(tr("Phone is not in coverage area"));
			else if(endResult == QStringLiteral("DELIVERY_ERROR"))
				returnError(tr("Request sending error"));
			else if(endResult == QStringLiteral("SIM_ERROR"))
				returnError(tr("SIM error"));
			else if(endResult == QStringLiteral("SIGNATURE_HASH_MISMATCH"))
				returnError(tr("Your mobile-ID transaction has failed. Please contact your mobile network operator."));
			else
				returnError(tr("Service result:") + endResult);
			return;
		}
		d->req.setUrl(QUrl(QStringLiteral("%1/signature/session/%2?timeoutMs=10000").arg(d->URL, d->sessionID)));
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
	if(!d->UUID.isEmpty() && QUuid(d->UUID).isNull())
	{
		WarningDialog::show(d->parentWidget(), tr("Failed to send request. Check your %1 service access settings.").arg(tr("mobile-ID")));
		return false;
	}
	d->ssid = ssid;
	d->cell = '+' + cell;
	d->info->setText(tr("Signing in process"));
	d->code->setAccessibleName(d->info->text());
	d->sessionID.clear();
	QByteArray data = QJsonDocument(QJsonObject::fromVariantHash(QVariantHash{
		{"relyingPartyUUID", d->UUID.isEmpty() ? QStringLiteral("00000000-0000-0000-0000-000000000000") : d->UUID},
		{"relyingPartyName", d->NAME},
		{"nationalIdentityNumber", d->ssid},
		{"phoneNumber", d->cell},
	})).toJson();
	d->req.setUrl(QUrl(QStringLiteral("%1/certificate").arg(d->URL)));
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

	d->code->setText(QStringLiteral("%1").arg((digest.front() >> 2) << 7 | (digest.back() & 0x7F), 4, 10, QChar('0')));
	d->info->setText(tr("Make sure control code matches with one in phone screen\n"
		"and enter mobile-ID PIN2-code."));
	d->code->setAccessibleName(QStringLiteral("%1 %2. %3").arg(d->controlCode->text(), d->code->text(), d->info->text()));

	QByteArray data = QJsonDocument(QJsonObject::fromVariantHash({
		{"relyingPartyUUID", d->UUID.isEmpty() ? QStringLiteral("00000000-0000-0000-0000-000000000000") : d->UUID},
		{"relyingPartyName", d->NAME},
		{"nationalIdentityNumber", d->ssid},
		{"phoneNumber", d->cell},
		{"hash", QByteArray::fromRawData((const char*)digest.data(), int(digest.size())).toBase64()},
		{"hashType", digestMethod},
		{"language", tr("ENG")},
		{"displayText", "%1"},
		{"displayTextFormat", "UCS-2"}
	})).toJson();
	// Workaround MID proxy issues
	data = QString::fromUtf8(data).arg(escapeUnicode(tr("Sign document"))).toUtf8();
	d->req.setUrl(QUrl(QStringLiteral("%1/signature").arg(d->URL)));
	qCDebug(MIDLog).noquote() << d->req.url() << data;
	d->manager->post(d->req, data);
	d->statusTimer->start();
	d->adjustSize();
	WaitDialogHider hider;
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

#include "MobileProgress.moc"
