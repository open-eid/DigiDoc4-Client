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

#define SIDV3DEMO

#include "SmartIDProgress.h"
#include "ui_SmartIDProgress.h"

#include "Application.h"
#include "CheckConnection.h"
#include "Crypto.h"
#include "Settings.h"
#include "dialogs/WarningDialog.h"

#include <digidocpp/crypto/X509Cert.h>

#include <QtCore/QCryptographicHash>
#include <QtCore/QDir>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QLoggingCategory>
#include <QtCore/QTimeLine>
#include <QtCore/QTimer>
#include <QtCore/QUuid>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>

#include "../QrCodeGenerator.h"

Q_LOGGING_CATEGORY(SIDLog,"RIA.SmartID")

using namespace digidoc;

#ifdef SIDV3DEMO
static constexpr std::string_view demo_cert(
"MIIGxjCCBa6gAwIBAgIQA4Y3b5+iF/PA/Jog/YdMiTANBgkqhkiG9w0BAQsFADBZ"
"MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMTMwMQYDVQQDEypE"
"aWdpQ2VydCBHbG9iYWwgRzIgVExTIFJTQSBTSEEyNTYgMjAyMCBDQTEwHhcNMjUw"
"OTI5MDAwMDAwWhcNMjYxMDEwMjM1OTU5WjBVMQswCQYDVQQGEwJFRTEQMA4GA1UE"
"BxMHVGFsbGlubjEbMBkGA1UEChMSU0sgSUQgU29sdXRpb25zIEFTMRcwFQYDVQQD"
"Ew5zaWQuZGVtby5zay5lZTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEB"
"AKAyy0yvjRCrATznThIwCu/wPCU5mV5UZIzNWl9KXx+gQiBp92SXfTOokkfiikBH"
"09HI+yVr3zI2U6FR8Tj21GiFE3bttmpCw8tJLmTe/P0Xah1D6vVkymbBt69N24ur"
"RqhW9in84WdkPc30vGJ+TdIj3jIePAbK3hHbpm+BfeyUhM48xXRgW+cBA//6R1C9"
"lUaF9Ycylf+g/P7FpmzHRk2HF3bPyWziBVOhIADtqMyVEJk20dl0SWGsCmAJuAhM"
"mOPc87zpXYzlAlY24XgsTyQdDnqmJn8ZukDahIt9ybKH/WPLkZfw6xBnsQKXdG0J"
"HBqBsgQdPDFsrsY45o4ek0kCAwEAAaOCA4wwggOIMB8GA1UdIwQYMBaAFHSFgMBm"
"x9833s+9KTeqAx2+7c0XMB0GA1UdDgQWBBSK7cmy40mto6zFVpcvnOyggb6YnzAZ"
"BgNVHREEEjAQgg5zaWQuZGVtby5zay5lZTA+BgNVHSAENzA1MDMGBmeBDAECAjAp"
"MCcGCCsGAQUFBwIBFhtodHRwOi8vd3d3LmRpZ2ljZXJ0LmNvbS9DUFMwDgYDVR0P"
"AQH/BAQDAgWgMB0GA1UdJQQWMBQGCCsGAQUFBwMBBggrBgEFBQcDAjCBnwYDVR0f"
"BIGXMIGUMEigRqBEhkJodHRwOi8vY3JsMy5kaWdpY2VydC5jb20vRGlnaUNlcnRH"
"bG9iYWxHMlRMU1JTQVNIQTI1NjIwMjBDQTEtMS5jcmwwSKBGoESGQmh0dHA6Ly9j"
"cmw0LmRpZ2ljZXJ0LmNvbS9EaWdpQ2VydEdsb2JhbEcyVExTUlNBU0hBMjU2MjAy"
"MENBMS0xLmNybDCBhwYIKwYBBQUHAQEEezB5MCQGCCsGAQUFBzABhhhodHRwOi8v"
"b2NzcC5kaWdpY2VydC5jb20wUQYIKwYBBQUHMAKGRWh0dHA6Ly9jYWNlcnRzLmRp"
"Z2ljZXJ0LmNvbS9EaWdpQ2VydEdsb2JhbEcyVExTUlNBU0hBMjU2MjAyMENBMS0x"
"LmNydDAMBgNVHRMBAf8EAjAAMIIBgAYKKwYBBAHWeQIEAgSCAXAEggFsAWoAdgDX"
"bX0Q0af1d8LH6V/XAL/5gskzWmXh0LMBcxfAyMVpdwAAAZmUn22IAAAEAwBHMEUC"
"IAanrRi7HrSbyOqc6s3QsP+S8ibPGe+g2pUuzEGb57EAAiEAi0496oVaL4EKdy1x"
"3g0gEivDSKVPKfn56YdyQ52GhbYAdwDCMX5XRRmjRe5/ON6ykEHrx8IhWiK/f9W1"
"rXaa2Q5SzQAAAZmUn20+AAAEAwBIMEYCIQCgHI7+yoKhNz7CZ9/5LezV38zyg/AD"
"2AwiQrrEfa9MSgIhAJ07CbJzN6TxII1Ow+NypN7aAlLw/p86gfafmgvKS+2BAHcA"
"lE5Dh/rswe+B8xkkJqgYZQHH0184AgE/cmd9VTcuGdgAAAGZlJ9tUwAABAMASDBG"
"AiEAuW7FVE3L1aNS83JHCFWaE0TgeaeOO7uYOw2hp4Hh1xsCIQC2iXVffo69rRnt"
"gnWvYZlXb/lIIawbcXXtXT0BPyFRbjANBgkqhkiG9w0BAQsFAAOCAQEAii5sJkW/"
"8qFlivqRf8L6HCxb0Q8wTeiFl/NGYuMkBL0RdRkLm14jGyCcFaQe6A3KLjDaLPa/"
"lSbBMRexH6r3oEzJeS8iuNMjEUKngyQh5PPOToO4Oi0rcHG2HjIMeOUAi7bHviu3"
"LOWMOmpoEtf1TRNQ7SBjOAj4qNbrVZCUoGdE2A9a/XOQZAyXaDwR89pEf898qMKY"
"mvht16vVX0g7FWkZ4X1ZQ/gqSjLyRwL/2B7mrgvsEeEU9nU0ZeM9Zsi/kKCJlmJF"
"G7nAniMUckeqRHX/RRGLQTCvE0RhnWcWbP54pi7XsyBDB3L9Uhw7szL9BE8Jtbuu"
"nnzTrMS+J2MQbQ==");
#endif

class SmartIDProgress::Private final: public QDialog, public Ui::SmartIDProgress
{
	Q_OBJECT
public:
	using QDialog::QDialog;

    ContainerPage *container;

    Private(ContainerPage *_container, QWidget *_parent) : QDialog(_parent), container(_container) {}

    void reject() final { l.exit(QDialog::Rejected); }
	void setVisible(bool visible) final {
		if(visible && !hider) hider = std::make_unique<WaitDialogHider>();
		QDialog::setVisible(visible);
		if(!visible && hider) hider.reset();
	}
	QTimer *timer {};
	QTimeLine *statusTimer {};
	QNetworkAccessManager *manager {};
	QNetworkRequest req;
	QString documentNumber, sessionID, sessionToken, deviceLinkBase, fileName;
	QByteArray sessionSecret;
	X509Cert cert;
	std::vector<unsigned char> signature;
	QEventLoop l;
	bool useCustomUUID = Settings::SID_UUID_CUSTOM;
    //QTimer *elapsedSecondsTimer {};
#ifdef SIDV3DEMO
	QString UUID = "00000000-0000-4000-8000-000000000000";
	QString NAME = "DEMO";
	QString URL = "https://sid.demo.sk.ee/smart-id-rp/v3";
#else
	QString UUID = useCustomUUID ? Settings::SID_UUID : QString();
	QString NAME = Settings::SID_NAME;
	QString URL = !UUID.isNull() && useCustomUUID ? Settings::SID_SK_URL : Settings::SID_PROXY_URL;
#endif
    qint64 startTimeStamp;
	std::unique_ptr<WaitDialogHider> hider;

    void httpFinished(QNetworkReply *reply);
    void timeSlice();
    void returnError (const QString &err, const QString &details = {});

    void httpError(QNetworkReply& reply);
    void endResultError(QString& result);

	QByteArray getAuthCode(QString unprotectedDeviceLink);
	QString getDeviceLink();
	QImage generateQr();
};

QByteArray
SmartIDProgress::Private::getAuthCode(QString unprotectedDeviceLink)
{
    QString schemeName = "smart-id-demo";
	QString relyingPartyName = "DEMO";
	QString brokeredRpName = "";
	QString authCodePayload =
		schemeName + "|" + // schemeName,
        "|" + // signatureProtocol
        "|" + // digest
        relyingPartyName.toUtf8().toBase64() + "|" + // relyingPartyNameBase64
        brokeredRpName.toUtf8().toBase64() + "|" + // brokeredRpNameBase64
        "|" + // interactions
        "|" + // initialCallbackUrl
        unprotectedDeviceLink;
    qCDebug(SIDLog).noquote() << "authCodePayload: " << authCodePayload;
    QByteArray authCode = Crypto::hmacSha256(sessionSecret, authCodePayload.toUtf8());

	return authCode;
}

QString
SmartIDProgress::Private::getDeviceLink()
{
    QString deviceLinkType = "QR";
    qint64 timestamp = QDateTime::currentSecsSinceEpoch();
    int elapsedSeconds = int(timestamp - startTimeStamp);
    QString sessionType = "cert";
    QString version = "1.0";
    QString lang = "eng";

	QString unprotectedDeviceLink = deviceLinkBase + "?" +
      "deviceLinkType=" + deviceLinkType +
      "&elapsedSeconds=" + QString::number(elapsedSeconds) +
      "&sessionToken=" + sessionToken +
      "&sessionType=" + sessionType +
      "&version=" + version +
      "&lang=" + lang;
    qCDebug(SIDLog).noquote() << "unprotectedDeviceLink: " << unprotectedDeviceLink;
    QString deviceLink = unprotectedDeviceLink +
      "&authCode=" + QString::fromUtf8(getAuthCode(unprotectedDeviceLink).toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
	return deviceLink;
}

void
SmartIDProgress::Private::timeSlice()
{
    qCDebug(SIDLog).noquote() << "timeSlice";
    if (!this->isVisible()) {
        qCDebug(SIDLog).noquote() << "showing";
        show();
    }
    if (!sessionID.isEmpty()) {
        qCDebug(SIDLog).noquote() << "generating";
        QString deviceLink = getDeviceLink();
        qCDebug(SIDLog).noquote() << "deviceLink: " << deviceLink;
        QrCodeGenerator gen;
        QImage img = gen.generateQr(deviceLink.toUtf8(), 256);
        //code->clear();
        code->setPixmap(QPixmap::fromImage(img));
    }
}

void
SmartIDProgress::Private::returnError (const QString &err, const QString &details)
{
    qCWarning(SIDLog) << err;
    statusTimer->stop();
    delete timer;
    timer = nullptr;
    hide();
    auto *dlg = WarningDialog::show(parentWidget(), err, details);
    QObject::connect(dlg, &WarningDialog::finished, &l, &QEventLoop::exit);
}

void
SmartIDProgress::Private::httpError(QNetworkReply& reply)
{
	const auto httpStatusCode = reply.attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
	qCWarning(SIDLog) << httpStatusCode << "Error :" << reply.error();
	qCDebug(SIDLog).noquote() << reply.readAll();

    switch(reply.error())
    {
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
        switch (httpStatusCode)
        {
        case 403:
            return returnError(tr("Failed to sign container. Check your %1 service access settings. "
                                  "<a href=\"https://www.id.ee/en/article/for-organisations-that-sign-large-quantities-of-documents-using-digidoc4-client/\">Additional information</a>").arg(tr("Smart-ID")));
        case 409:
            return returnError(tr("Failed to send request. The number of unsuccesful request from this IP address has been exceeded. Please try again later."));
        case 429:
            return returnError(tr("The limit for %1 digital signatures per month has been reached. "
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
            return returnError(tr("Failed to send request. %1 service has encountered technical errors. Please try again later.").arg(QLatin1String("Smart-ID")), reply.errorString());
        }
    }
}

void
SmartIDProgress::Private::endResultError(QString& endResult)
{
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
    returnError(tr("Service result: ") + endResult);
}

void
SmartIDProgress::Private::httpFinished(QNetworkReply *reply) {
	QScopedPointer<QNetworkReply,QScopedPointerDeleteLater> scope(reply);
    if (reply->error() != QNetworkReply::NoError) {
        // http error
        return httpError(*reply);
    }
	if(!reply->header(QNetworkRequest::ContentTypeHeader).toString().startsWith(QLatin1String("application/json")))
		return returnError(tr("Invalid content type header ") + reply->header(QNetworkRequest::ContentTypeHeader).toString());

    // Result 200, get reply body as JSON
	QByteArray data = reply->readAll();
	reply->deleteLater();
	qCDebug(SIDLog).noquote() << data;
	QJsonObject result = QJsonDocument::fromJson(data, nullptr).object();
	if(result.isEmpty()) {
		return returnError(tr("Failed to parse JSON content"));
	}

    // Have reply from RP
	if(sessionID.isEmpty()) {
        // Initial request, save session parameters
        qCDebug(SIDLog).noquote() << "New session";
		sessionID = result.value(QLatin1String("sessionID")).toString();
        qCDebug(SIDLog).noquote() << "sessionID: " << sessionID;
		sessionToken = result.value(QLatin1String("sessionToken")).toString();
        qCDebug(SIDLog).noquote() << "SessionToken: " << sessionToken;
        QString secret64 = result.value(QLatin1String("sessionSecret")).toString();
        sessionSecret = QByteArray::fromBase64(secret64.toUtf8());
        qCDebug(SIDLog).noquote() << "sessionSecret: " << secret64;
        deviceLinkBase = result.value(QLatin1String("deviceLinkBase")).toString();
        qCDebug(SIDLog).noquote() << "deviceLinkBase: " << deviceLinkBase;
		if (!sessionToken.isEmpty()) {
			// Certificate selection session
			qCDebug(SIDLog).noquote() << "Starting QR timer";
			startTimeStamp = QDateTime::currentSecsSinceEpoch();
			timeSlice();
		}
	} else {
		if (result.value(QLatin1String("state")).toString() != QLatin1String("RUNNING")) {
            // Disable Qr generation
            qCDebug(SIDLog).noquote() << "Disabling Qr timer";
            timer->stop();
			QString endResult = result.value(QLatin1String("result")).toObject().value(QLatin1String("endResult")).toString();
            if (endResult != QLatin1String("OK")) {
                return endResultError(endResult);
            }
            if(documentNumber.isEmpty()) {
                documentNumber = result.value(QLatin1String("result")).toObject().value(QLatin1String("documentNumber")).toString();
            }
            if(result.contains(QLatin1String("signature"))) {
                QJsonObject r_sig = result.value(QLatin1String("signature")).toObject();
                if (r_sig.contains(QLatin1String("value"))) {
                    QByteArray b64 = QByteArray::fromBase64(r_sig.value(QLatin1String("value")).toString().toUtf8());
                    signature.assign(b64.cbegin(), b64.cend());
                    hide();
                    l.exit(QDialog::Accepted);
                    return;
                }
            }
            if(result.contains(QLatin1String("cert"))) {
                try {
                    QByteArray b64 = QByteArray::fromBase64(result.value(QLatin1String("cert")).toObject().value(QLatin1String("value")).toString().toUtf8());
                    cert = X509Cert((const unsigned char*)b64.constData(), size_t(b64.size()), X509Cert::Der);
                    hide();
                    l.exit(QDialog::Accepted);
                } catch(const Exception &e) {
                    returnError(tr("Failed to parse certificate: ") + QString::fromStdString(e.msg()));
                }
            }
            return;
		}
	}
	req.setUrl(QUrl(QStringLiteral("%1/session/%2?timeoutMs=10000").arg(URL, sessionID)));
	qCDebug(SIDLog).noquote() << req.url();
	manager->get(req);
}

SmartIDProgress::SmartIDProgress(ContainerPage *container, QWidget *parent)
    : d(new Private(container, parent))
{
#if 0
	const_cast<QLoggingCategory&>(SIDLog()).setEnabled(QtDebugMsg,
		QFile::exists(QStringLiteral("%1/%2.log").arg(QDir::tempPath(), QApplication::applicationName())));
#else
	const_cast<QLoggingCategory&>(SIDLog()).setEnabled(QtDebugMsg, true);
	qCDebug(SIDLog).noquote() << "Tere Talv!";
#endif
	d->setWindowFlags(Qt::Dialog|Qt::CustomizeWindowHint);
	d->setupUi(d);
	d->signProgressBar->setMaximum(100);
	d->code->setBuddy(d->signProgressBar);
    //d->code->clear();
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
	QObject::connect(d->cancel, &QPushButton::clicked, d, &QDialog::reject);

	d->statusTimer = new QTimeLine(d->signProgressBar->maximum() * 1000, d);
	d->statusTimer->setEasingCurve(QEasingCurve::Linear);
	d->statusTimer->setFrameRange(d->signProgressBar->minimum(), d->signProgressBar->maximum());
	QObject::connect(d->statusTimer, &QTimeLine::frameChanged, d->signProgressBar, &QProgressBar::setValue);

	d->req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
#ifdef SIDV3DEMO
	QByteArray cba = QByteArray::fromBase64(demo_cert.data());
	d->manager = CheckConnection::setupNAM(d->req, cba);
#else
	d->manager = CheckConnection::setupNAM(d->req);
#endif
	d->manager->setParent(d);
    QNetworkAccessManager::connect(d->manager, &QNetworkAccessManager::finished, d, &SmartIDProgress::Private::httpFinished);
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
	if(!d->UUID.isEmpty() && QUuid(d->UUID).isNull()) {
		WarningDialog::show(d->parentWidget(), tr("Failed to send request. Check your %1 service access settings.").arg(tr("Smart-ID")));
		return false;
	}
	QFileInfo info(fileName);
	QString displayFile = info.completeBaseName();
	if(displayFile.size() > 6)
		displayFile = displayFile.left(3) + QStringLiteral("...") + displayFile.right(3);
	displayFile += QStringLiteral(".") + info.suffix();
	d->fileName = displayFile;
	qCDebug(SIDLog).noquote() << "fileName: " << d->fileName;
	d->sessionID.clear();
	QByteArray data = QJsonDocument({
		{"relyingPartyUUID", d->UUID.isEmpty() ? QStringLiteral("00000000-0000-0000-0000-000000000000") : d->UUID},
		{"relyingPartyName", d->NAME},
		{"certificateLevel", "QUALIFIED"},
		{"nonce", QUuid::createUuid().toString().remove('-').mid(1, 30)}
	}).toJson();
	d->req.setUrl(QUrl(QStringLiteral("%1/signature/certificate-choice/device-link/anonymous").arg(d->URL)));
	qCDebug(SIDLog).noquote() << d->req.url() << data;
	d->manager->post(d->req, data);
    d->info->setText(tr("Open the Smart-ID application on your smart device and scan the QR code."));
	d->code->setAccessibleName(d->info->text());
	d->statusTimer->start();
	d->adjustSize();
	d->timer = new QTimer(d);
    d->timer->setInterval(1000);
    QObject::connect(d->timer, &QTimer::timeout, d, &SmartIDProgress::Private::timeSlice);
	d->timer->start();
	return d->l.exec() == QDialog::Accepted;
}

std::vector<unsigned char> SmartIDProgress::sign(const std::string &method, const std::vector<unsigned char> &digest) const
{
	QString digestMethod;
	if(method == "http://www.w3.org/2001/04/xmldsig-more#rsa-sha256")
		digestMethod = QStringLiteral("SHA-256");
	else if(method == "http://www.w3.org/2001/04/xmldsig-more#rsa-sha384")
		digestMethod = QStringLiteral("SHA-384");
	else if(method == "http://www.w3.org/2001/04/xmldsig-more#rsa-sha512")
		digestMethod = QStringLiteral("SHA-512");
	else
		throw Exception(__FILE__, __LINE__, "Unsupported digest method");

	QJsonArray interactions {
		QJsonObject{
			{"type", "confirmationMessage"},
			{"displayText200", QStringLiteral("%1 %2").arg(tr("Sign document"), d->fileName)}
		},
		QJsonObject{
			{"type", "displayTextAndPIN"},
			{"displayText60", QStringLiteral("%1 %2").arg(tr("Sign document"), d->fileName)}
		}
	};

	QJsonObject req{
		{"relyingPartyUUID", (d->UUID.isEmpty() ? QStringLiteral("00000000-0000-0000-0000-000000000000") : d->UUID)},
		{"relyingPartyName", d->NAME},
		{"certificateLevel", "QUALIFIED"},
		{"signatureProtocol", "RAW_DIGEST_SIGNATURE"},
		{"signatureProtocolParameters", QJsonObject{
    		{"digest", QString(QByteArray::fromRawData((const char*)digest.data(), int(digest.size())).toBase64())},
    		{"signatureAlgorithm", "rsassa-pss"},
    		{"signatureAlgorithmParameters", QJsonObject{
      			{"hashAlgorithm", digestMethod}
    		}}
  		}},
		{"linkedSessionID", d->sessionID},
		{"nonce", QUuid::createUuid().toString().remove('-').mid(1, 30)},
		{"interactions", QString::fromLatin1(QJsonDocument(interactions).toJson().toBase64())}
	};
	QByteArray data = QJsonDocument(req).toJson();

	d->req.setUrl(QUrl(QStringLiteral("%1/signature/notification/linked/%2").arg(d->URL, d->documentNumber)));
	qCDebug(SIDLog).noquote() << d->req.url() << data;

	d->statusTimer->stop();
	d->sessionID.clear();

	d->manager->post(d->req, data);
	d->statusTimer->start();
    d->code->setText(tr("Please confirm signing on your smart device"));
	d->label->hide();
	d->info->hide();
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
