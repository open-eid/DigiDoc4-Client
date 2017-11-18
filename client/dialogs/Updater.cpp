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

#include "Updater.h"
#include "ui_Updater.h"
#include "QSmartCard_p.h"
#include "Styles.h"
#include "effects/Overlay.h"

#include "common/Common.h"
#include "common/Configuration.h"
#include "common/QPCSC.h"
#include "common/PinDialog.h"
#include "common/Settings.h"
#include "common/SslCertificate.h"

#include <QtCore/QTimer>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QTimeLine>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkProxy>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSslKey>
#include <QtGui/QPainter>
#include <QtWidgets/QPushButton>

#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/ecdsa.h>

#include <memory>
#include <thread>

#if OPENSSL_VERSION_NUMBER < 0x10010000L
static int ECDSA_SIG_set0(ECDSA_SIG *sig, BIGNUM *r, BIGNUM *s)
{
	if(!r || !s)
		return 0;
	BN_clear_free(sig->r);
	BN_clear_free(sig->s);
	sig->r = r;
	sig->s = s;
	return 1;
}
#endif

class UpdaterPrivate: public Ui::Updater
{
public:
	QPCSCReader *reader = nullptr;
	QPushButton *close = nullptr, *details = nullptr;
#if OPENSSL_VERSION_NUMBER < 0x10010000L || defined(LIBRESSL_VERSION_NUMBER)
	RSA_METHOD rsamethod = *RSA_get_default_method();
	ECDSA_METHOD *ecmethod = ECDSA_METHOD_new(nullptr);
#else
	RSA_METHOD *rsamethod = RSA_meth_dup(RSA_get_default_method());
	EC_KEY_METHOD *ecmethod = EC_KEY_METHOD_new(nullptr);
#endif
	QSslCertificate cert;
	QString session;
	QNetworkRequest request;
	void setButtonPattern(QPushButton *button, const QString &color) const;
	QPCSCReader::Result verifyPIN(const QString &title, int p1) const;
	QtMessageHandler oldMsgHandler = nullptr;
	QTimeLine *statusTimer = nullptr;

	static QByteArray sign(const unsigned char *dgst, int digst_len, UpdaterPrivate *d)
	{
		if(!d || !d->reader || !d->reader->connect())
			return QByteArray();

		if(!d->reader->beginTransaction())
		{
			d->reader->disconnect();
			return QByteArray();
		}

		// Set card parameters
		if(!d->reader->transfer(APDU("0022F301 00")).resultOk() || // SecENV 1
			!d->reader->transfer(APDU("002241B8 02 8300")).resultOk()) //Key reference, 8303801100
		{
			d->reader->endTransaction();
			d->reader->disconnect();
			return QByteArray();
		}

		// calc signature
		QByteArray cmd = APDU("00880000 00");
		cmd[4] = char(digst_len);
		cmd += QByteArray::fromRawData((const char*)dgst, digst_len);
		QPCSCReader::Result result = d->reader->transfer(cmd);
		d->reader->endTransaction();
		d->reader->disconnect();
		if(!result)
			return QByteArray();
		return result.data;
	}

	static int rsa_sign(int type, const unsigned char *m, unsigned int m_len,
		unsigned char *sigret, unsigned int *siglen, const RSA *rsa)
	{
		if(type != NID_md5_sha1 || m_len != 36)
			return 0;
		QByteArray result = sign(m, int(m_len), (UpdaterPrivate*)RSA_get_app_data(rsa));
		if(result.isEmpty())
			return 0;
		*siglen = (unsigned int)result.size();
		memcpy(sigret, result.constData(), size_t(result.size()));
		return 1;
	}

	static ECDSA_SIG* ecdsa_do_sign(const unsigned char *dgst, int dgst_len,
		const BIGNUM *inv, const BIGNUM *rp, EC_KEY *eckey)
	{
#if OPENSSL_VERSION_NUMBER < 0x10010000L
		UpdaterPrivate *d = (UpdaterPrivate*)EC_KEY_get_key_method_data(eckey, nullptr, nullptr, nullptr);
#else
		UpdaterPrivate *d = (UpdaterPrivate*)EC_KEY_get_ex_data(eckey, 0);
#endif
		QByteArray result = sign(dgst, dgst_len, d);
		if(result.isEmpty())
			return nullptr;
		QByteArray r = result.left(result.size()/2);
		QByteArray s = result.right(result.size()/2);
		ECDSA_SIG *sig = ECDSA_SIG_new();
		ECDSA_SIG_set0(sig,
			BN_bin2bn((const unsigned char*)r.data(), int(r.size()), 0),
			BN_bin2bn((const unsigned char*)s.data(), int(s.size()), 0));
		return sig;
	}
};

void UpdaterPrivate::setButtonPattern(QPushButton *button, const QString &color) const
{
	QFont condensed14 = Styles::font( Styles::Condensed, 14 );
	if( color != nullptr )
	{	// Red color styled button
		button->setStyleSheet( 
			"QPushButton { border-radius: 2px; border: none; color: #ffffff; background-color: #981E32;}" 
			"QPushButton:pressed { background-color: #F24A66;}" 
			"QPushButton:hover:!pressed { background-color: #CD2541;}" 
			); 
	}
	else
	{	// Blue color styled button
		button->setStyleSheet( 
			"QPushButton { border-radius: 2px; border: none; color: #ffffff; background-color: #006EB5;}" 
			"QPushButton:pressed { background-color: #41B6E6;}" 
			"QPushButton:hover:!pressed { background-color: #008DCF;}" 
			"QPushButton:disabled { background-color: #BEDBED;};" 
			); 
	}
	button->setMinimumHeight( 30 );
	button->setMinimumWidth( 120 );
	button->setFont( condensed14 );
	button->update();
}

QPCSCReader::Result UpdaterPrivate::verifyPIN(const QString &title, int p1) const
{
	stackedWidget->setCurrentIndex(3);
	QRegExp regexp;
	QString text = "<b>" + title + "</b><br />";
	if(p1 == 2)
	{
		text += PinDialog::tr("Selected action requires sign certificate.") + "<br />";
		text += reader->isPinPad() ?
			PinDialog::tr("For using sign certificate enter PIN2 at the reader") :
			PinDialog::tr("For using sign certificate enter PIN2");
		regexp.setPattern( "\\d{5,12}" );
		pinType->setText("PIN2");
	}
	else
	{
		text += PinDialog::tr("Selected action requires authentication certificate.") + "<br />";
		text += reader->isPinPad() ?
			PinDialog::tr("For using authentication certificate enter PIN1 at the reader") :
			PinDialog::tr("For using authentication certificate enter PIN1");
		regexp.setPattern( "\\d{4,12}" );
		pinType->setText("PIN1");
	}
	pinInput->setHidden(reader->isPinPad());
	pinProgress->setVisible(reader->isPinPad());
	QEventLoop l;
	QPushButton *okButton = nullptr, *cancelButton = nullptr;
	if(!reader->isPinPad())
	{
		okButton = buttonBox->addButton(::Updater::tr("CONTINUE"), QDialogButtonBox::AcceptRole);
		cancelButton = buttonBox->addButton(::Updater::tr("CANCEL"), QDialogButtonBox::RejectRole);
		setButtonPattern( okButton,  nullptr );
		setButtonPattern( cancelButton,  "Red" );
		okButton->setDisabled(true);
		pinInput->setValidator(new QRegExpValidator(regexp, pinInput));
		::Updater::connect(pinInput, &QLineEdit::textEdited, okButton, [&](const QString &text){
			okButton->setEnabled(regexp.exactMatch(text));
		});
		::Updater::connect(okButton, &QPushButton::clicked, &l, [&]{ l.exit(1); });
		::Updater::connect(cancelButton, &QPushButton::clicked, &l, [&]{ l.exit(0); });
	}
	close->hide();
	details->hide();

	TokenData::TokenFlags token = 0;
	Q_FOREVER
	{
		QString error;
		if(token & TokenData::PinFinalTry)
			error = "<br /><font color='red'><b>" + PinDialog::tr("PIN will be locked next failed attempt") + "</b></font>";
		else if(token & TokenData::PinCountLow)
			error = "<br /><font color='red'><b>" + PinDialog::tr("PIN has been entered incorrectly one time") + "</b></font>";
		pinLabel->setText(text + error + "<br />");
		Common::setAccessibleName(pinLabel);

		QByteArray verify = APDU("00200000 00");
		verify[3] = p1;
		QPCSCReader::Result result;
		if(reader->isPinPad())
		{
			pinProgress->setValue(pinProgress->maximum());
			std::thread([&]{
				result = reader->transferCTL(verify, true);
				l.quit();
			}).detach();
			statusTimer->start();
			l.exec();
		}
		else
		{
			pinInput->clear();
			pinInput->setFocus();
			if(l.exec() == 1)
			{
				verify[4] = pinInput->text().size();
				result = reader->transfer(verify + pinInput->text().toUtf8());
			}
		}
		switch( (quint8(result.SW[0]) << 8) + quint8(result.SW[1]) )
		{
		case 0x63C1: token = TokenData::PinFinalTry; continue; // Validate error, 1 tries left
		case 0x63C2: token = TokenData::PinCountLow; continue; // Validate error, 2 tries left
		case 0x63C3: continue;
		case 0x63C0: // Blocked
		case 0x6400: // Timeout
		case 0x6401: // Cancel
		case 0x6402: // Mismatch
		case 0x6403: // Lenght error
		case 0x6983: // Blocked
		case 0x9000: // No error
		default:
			stackedWidget->setCurrentIndex(0);
			if(okButton)
			{
				buttonBox->removeButton(okButton);
				okButton->deleteLater();
			}
			if(cancelButton)
			{
				buttonBox->removeButton(cancelButton);
				cancelButton->deleteLater();
			}
			close->show();
			details->show();
			return result;
		}
	}
}



Updater::Updater(const QString &reader, QWidget *parent)
	: QDialog(parent)
	, d(new UpdaterPrivate)
{
	d->setupUi(this);
	setWindowFlags( Qt::Dialog | Qt::FramelessWindowHint );
	setWindowModality( Qt::ApplicationModal );
	d->statusTimer = new QTimeLine(d->pinProgress->maximum() * 1000, d->pinProgress);
	d->statusTimer->setCurveShape(QTimeLine::LinearCurve);
	d->statusTimer->setFrameRange(d->pinProgress->maximum(), d->pinProgress->minimum());
	connect(d->statusTimer, &QTimeLine::frameChanged, d->pinProgress, &QProgressBar::setValue);
	connect(this, &Updater::start, this, &Updater::run, Qt::QueuedConnection);

	d->reader = new QPCSCReader(reader, &QPCSC::instance());

#if OPENSSL_VERSION_NUMBER < 0x10010000L || defined(LIBRESSL_VERSION_NUMBER)
	d->rsamethod.name = "Updater";
	d->rsamethod.rsa_sign = UpdaterPrivate::rsa_sign;
	ECDSA_METHOD_set_app_data(d->ecmethod, d);
	ECDSA_METHOD_set_name(d->ecmethod, const_cast<char*>("QSmartCard"));
	ECDSA_METHOD_set_sign(d->ecmethod, UpdaterPrivate::ecdsa_do_sign);
#else
	RSA_meth_set1_name(d->rsamethod, "Updater");
	RSA_meth_set_sign(d->rsamethod, UpdaterPrivate::rsa_sign);
	EC_KEY_METHOD_set_sign(d->ecmethod, nullptr, nullptr, UpdaterPrivate::ecdsa_do_sign);
#endif

	QFont regular = Styles::font( Styles::Regular, 13 );
	QFont condensed14 = Styles::font( Styles::Condensed, 14 );

	d->line->hide();
	d->label->setFont( regular );
	d->progressRunning->setFont( regular );
	d->lblHeader->setFont( condensed14 );
	d->message->setFont( regular );
	d->buttonBox->setMinimumHeight(30);
	d->messageAgree->setFont( regular );
	d->envelope->setFont( regular );
	d->envelopeLabel->setFont( regular );
	d->envelopeAgree->setFont( regular );
	d->pinLabel->setFont( condensed14 );
	d->pinType->setFont( condensed14 );
	d->details = d->buttonBox->addButton(tr("DETAILS"), QDialogButtonBox::ActionRole);
	d->setButtonPattern( d->details,  nullptr );
	d->details->hide();
	d->close = d->buttonBox->button(QDialogButtonBox::Close);
	d->setButtonPattern( d->close,  "Red" );
	d->close->hide();
	d->log->hide();
	connect(d->details, &QPushButton::clicked, [=]{
		d->log->setVisible(!d->log->isVisible());
		if(d->progressRunning)
			d->progressRunning->setVisible(d->log->isHidden());
	});
	connect(d->close, &QPushButton::clicked, this, &Updater::accept);
	connect(this, &Updater::log, d->log, &QPlainTextEdit::appendPlainText, Qt::QueuedConnection);

	static Updater *instance = nullptr;
	instance = this;
	d->oldMsgHandler = qInstallMessageHandler([](QtMsgType, const QMessageLogContext &, const QString &msg){
		if(!msg.contains("QObject")) //Silence Qt warnings
			Q_EMIT instance->log(msg);
	});
}

Updater::~Updater()
{
	d->reader->endTransaction();
	delete d->reader;
	qInstallMessageHandler(d->oldMsgHandler);
#if OPENSSL_VERSION_NUMBER >= 0x10010000L
	RSA_meth_free(d->rsamethod);
	EC_KEY_METHOD_free(d->ecmethod);
#else
	ECDSA_METHOD_free(d->ecmethod);
#endif
	delete d;
}

void Updater::process(const QByteArray &data)
{
#if QT_VERSION >= 0x050400
	qDebug().noquote() << ">" << data;
#else
	qDebug() << ">" << data;
#endif
	QJsonObject obj = QJsonDocument::fromJson(data).object();

	if(d->session.isEmpty())
		d->session = obj.value("session").toString();
	QString cmd = obj.value("cmd").toString();
	if(cmd == "CONNECT")
	{
		QPCSCReader::Mode mode = QPCSCReader::Mode(QPCSCReader::T0|QPCSCReader::T1);
		if(obj.value("protocol").toString() == "T=0") mode = QPCSCReader::T0;
		if(obj.value("protocol").toString() == "T=1") mode = QPCSCReader::T1;
		quint32 err = 0;
#ifdef Q_OS_WIN
		err = d->reader->connectEx(QPCSCReader::Exclusive, mode);
#else
		if((err = d->reader->connectEx(QPCSCReader::Exclusive, mode)) == 0 ||
			(err = d->reader->connectEx(QPCSCReader::Shared, mode)) == 0)
			d->reader->beginTransaction();
#endif
		QVariantHash ret{
			{"CONNECT", d->reader->isConnected() ? "OK" : "NOK"},
			{"reader", d->reader->name()},
			{"atr", d->reader->atr()},
			{"protocol", d->reader->protocol() == 2 ? "T=1" : "T=0"},
			{"pinpad", d->reader->isPinPad()}
		};
		if(err)
			ret["ERROR"] = QString::number(err, 16);
		Q_EMIT send(ret);
	}
	else if(cmd == "DISCONNECT")
	{
		d->reader->endTransaction();
		d->reader->disconnect([](const QString &action) {
			if(action == "leave") return QPCSCReader::LeaveCard;
			if(action == "eject") return QPCSCReader::EjectCard;
			return QPCSCReader::ResetCard;
		}(obj.value("action").toString()));
		Q_EMIT send({{"DISCONNECT", "OK"}});
	}
	else if(cmd == "APDU")
	{
		std::thread([=]{
			QPCSCReader::Result result = d->reader->transfer(APDU(obj.value("bytes").toString().toLatin1()));
			QVariantHash ret;
			ret["APDU"] = result.err ? "NOK" : "OK";
			ret["bytes"] = QByteArray(result.data + result.SW).toHex();
			if(result.err)
				ret["ERROR"] = QString::number(result.err, 16);
			Q_EMIT send(ret);
		}).detach();
	}
	else if(cmd == "MESSAGE")
	{
		d->label->setText(obj.value("text").toString());
		Q_EMIT send({{"MESSAGE", "OK"}});
	}
	else if(cmd == "DIALOG")
	{
		d->stackedWidget->setCurrentIndex(1);
		d->message->setText(obj.value("text").toString());
		Common::setAccessibleName(d->message);
		QPushButton *noButton = d->buttonBox->addButton(tr("CANCEL"), QDialogButtonBox::RejectRole);
		QPushButton *yesButton = d->buttonBox->addButton(tr("START"), QDialogButtonBox::AcceptRole);
		yesButton->setDisabled(true);
		d->setButtonPattern( yesButton, nullptr );
		d->setButtonPattern( noButton, "Red" );
		QEventLoop l;
		connect(d->messageAgree, &QCheckBox::toggled, yesButton, &QPushButton::setEnabled);
		connect(yesButton, &QPushButton::clicked, [&]{ l.exit(1); });
		connect(noButton, &QPushButton::clicked, [&]{ l.exit(0); reject(); });
		d->details->hide();
		d->close->hide();
		Q_EMIT send({{"DIALOG", "OK"}, {"button", l.exec() == 1 ? "green" : "red"}});
		d->buttonBox->removeButton(yesButton);
		yesButton->deleteLater();
		d->buttonBox->removeButton(noButton);
		noButton->deleteLater();
		d->stackedWidget->setCurrentIndex(0);
		d->details->show();
	}
	else if(cmd == "VERIFY")
	{
		QPCSCReader::Result result = d->verifyPIN(obj.value("text").toString(), obj.value("p2").toInt(1));
		Q_EMIT send({
			{"VERIFY", result.resultOk() ? "OK" : "NOK"},
			{"bytes", QByteArray(result.data + result.SW).toHex()}
		});
	}
	else if(cmd == "DECRYPT")
	{
		QPCSCReader::Result result = d->reader->transfer(APDU(obj.value("bytes").toString().toLatin1()));
		if(result.resultOk())
		{
			QPixmap pinEnvelope(QSize(d->message->width(), 100));
			QPainter p(&pinEnvelope);
			p.fillRect(pinEnvelope.rect(), Qt::white);
			p.setPen(Qt::black);
			int pos = result.data.lastIndexOf('#');
			if(pos != -1)
				result.data = result.data.mid(0, pos - 2);
			p.drawText(pinEnvelope.rect(), Qt::AlignCenter, QString::fromUtf8(result.data));
			d->envelope->setPixmap(pinEnvelope);
			d->envelopeLabel->setText(obj.value("text").toString());
			d->stackedWidget->setCurrentIndex(2);
			QPushButton *yesButton = d->buttonBox->addButton(tr("CONTINUE"), QDialogButtonBox::AcceptRole);
			QPushButton *cancelButton = d->buttonBox->addButton(tr("CANCEL"), QDialogButtonBox::RejectRole);
			d->setButtonPattern( yesButton, nullptr );
			d->setButtonPattern( cancelButton, "Red" );
			yesButton->setDisabled(true);
			QEventLoop l;
			connect(d->envelopeAgree, &QCheckBox::toggled, yesButton, &QPushButton::setEnabled);
			connect(yesButton, &QPushButton::clicked, [&]{ l.exit(1); });
			connect(cancelButton, &QPushButton::clicked, [&]{ l.exit(0); });
			d->details->hide();
			d->close->hide();
			Q_EMIT send({{"DECRYPT", "OK"}, {"button", l.exec() == 1 ? "green" : "red"}});
			d->buttonBox->removeButton(yesButton);
			yesButton->deleteLater();
			d->buttonBox->removeButton(cancelButton);
			cancelButton->deleteLater();
			d->stackedWidget->setCurrentIndex(0);
			d->details->show();
		}
		else
		{
			QVariantHash ret;
			ret["DECRYPT"] = "NOK";
			ret["bytes"] = QByteArray(result.data + result.SW).toHex();
			if(result.err)
				ret["ERROR"] = QString::number(result.err, 16);
			Q_EMIT send(ret);
		}
	}
	else if(cmd == "STOP")
	{
		d->progressBar->hide();
		d->progressRunning->deleteLater();
		d->progressRunning = nullptr;
		if(obj.contains("text"))
			d->label->setText(obj.value("text").toString());
		d->close->show();
	}
	else
		Q_EMIT send({{"CMD", "UNKNOWN"}});
}

int Updater::exec()
{
	d->stackedWidget->setCurrentIndex(1);
	// Read certificate
	d->reader->connect();
	d->reader->beginTransaction();
	if(!d->reader->transfer(APDU("00A40000 00")).resultOk())
	{
		// Master file selection failed, test if it is updater applet
		d->reader->transfer(APDU("00A40400 0A D2330000005550443101"));
		d->reader->transfer(APDU("00A40000 00"));
	}
	d->reader->transfer(APDU("00A40000 00"));
	d->reader->transfer(APDU("00A40100 02 EEEE"));
	QPCSCReader::Result data = d->reader->transfer(APDU("00A40200 02 AACE"));
	QHash<quint8,QByteArray> fci = QSmartCardPrivate::parseFCI(data.data);
	int size = fci.contains(0x85) ? fci[0x85][0] << 8 | fci[0x85][1] : 0x0600;
	QByteArray certData;
	while(certData.size() < size)
	{
		QByteArray apdu = APDU("00B00000 00");
		apdu[2] = certData.size() >> 8;
		apdu[3] = certData.size();
		QPCSCReader::Result result = d->reader->transfer(apdu);
		if(!result.resultOk())
		{
			d->reader->endTransaction();
			d->label->setText(tr("Failed to read certificate"));
			return QDialog::exec();
		}
		certData += result.data;
	}

	d->reader->endTransaction();
	d->reader->disconnect();

	// Associate certificate and key with operation.
	d->cert = QSslCertificate(certData, QSsl::Der);
	EVP_PKEY *key = nullptr;
	if(!d->cert.isNull())
	{
		if (d->cert.publicKey().algorithm() == QSsl::Ec)
		{
			EC_KEY *ec = EC_KEY_dup((EC_KEY*)d->cert.publicKey().handle());
#if OPENSSL_VERSION_NUMBER < 0x10010000L
			EC_KEY_insert_key_method_data(ec, d, nullptr, nullptr, nullptr);
			ECDSA_set_method(ec, d->ecmethod);
#else
			EC_KEY_set_ex_data(ec, 0, d);
			EC_KEY_set_method(ec, d->ecmethod);
#endif
			EVP_PKEY *key = EVP_PKEY_new();
			EVP_PKEY_set1_EC_KEY(key, ec);
			//EC_KEY_free(ec);
		}
		else
		{
			RSA *rsa = RSAPublicKey_dup((RSA*)d->cert.publicKey().handle());
#if OPENSSL_VERSION_NUMBER < 0x10010000L || defined(LIBRESSL_VERSION_NUMBER)
			RSA_set_method(rsa, &d->rsamethod);
			rsa->flags |= RSA_FLAG_SIGN_VER;
#else
			RSA_set_method(rsa, d->rsamethod);
#endif
			RSA_set_app_data(rsa, d);
			key = EVP_PKEY_new();
			EVP_PKEY_set1_RSA(key, rsa);
			//RSA_free(rsa);
		}
	}

	// Do connection
	QNetworkAccessManager *net = new QNetworkAccessManager(this);
	d->request = QNetworkRequest(QUrl(
		Configuration::instance().object().value("EIDUPDATER-URL-TOECC").toString()));
	d->request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	d->request.setRawHeader("User-Agent", QString("%1/%2 (%3)")
		.arg(qApp->applicationName(), qApp->applicationVersion(), Common::applicationOs()).toUtf8());
	qDebug() << "Connecting to" << d->request.url().toString();

	QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();
	QList<QSslCertificate> trusted;
	for(const QJsonValue &cert: Configuration::instance().object().value("CERT-BUNDLE").toArray())
		trusted << QSslCertificate(QByteArray::fromBase64(cert.toString().toLatin1()), QSsl::Der);
	ssl.setCaCertificates(QList<QSslCertificate>());
	ssl.setProtocol(QSsl::TlsV1_0);
	if(key)
	{
		ssl.setPrivateKey(QSslKey(key));
		ssl.setLocalCertificate(d->cert);
	}
	d->request.setSslConfiguration(ssl);

	// Get proxy settings
	QNetworkProxy proxy = []() -> const QNetworkProxy {
		for(const QNetworkProxy &proxy: QNetworkProxyFactory::systemProxyForQuery())
			if(proxy.type() == QNetworkProxy::HttpProxy)
				return proxy;
		return QNetworkProxy();
	}();
	Settings s(qApp->applicationName());
	QString proxyHost = s.value("PROXY-HOST").toString();
	if(!proxyHost.isEmpty())
	{
		proxy.setHostName(proxyHost.split(':').at(0));
		proxy.setPort(proxyHost.split(':').at(1).toUInt());
	}
	proxy.setUser(s.value("PROXY-USER", proxy.user()).toString());
	proxy.setPassword(s.value("PROXY-PASS", proxy.password()).toString());
	proxy.setType(QNetworkProxy::HttpProxy);
	net->setProxy(proxy.hostName().isEmpty() ? QNetworkProxy() : proxy);
	qDebug() << "Proxy" << proxy.hostName() << ":" << proxy.port() << "User" << proxy.user();

	connect(net, &QNetworkAccessManager::sslErrors, this, [=](QNetworkReply *reply, const QList<QSslError> &errors){
		QList<QSslError> ignore;
		for(const QSslError &error: errors)
		{
			switch(error.error())
			{
			case QSslError::UnableToGetLocalIssuerCertificate:
			case QSslError::CertificateUntrusted:
				if(trusted.contains(reply->sslConfiguration().peerCertificate()))
					ignore << error;
				break;
			default: break;
			}
		}
		reply->ignoreSslErrors(ignore);
	});
	connect(this, &Updater::send, net, [=](const QVariantHash &response){
		QJsonObject resp;
		if(!d->session.isEmpty())
			resp["session"] = d->session;
		for(QVariantHash::const_iterator i = response.constBegin(); i != response.constEnd(); ++i)
			resp[i.key()] = QJsonValue::fromVariant(i.value());
		QByteArray data = QJsonDocument(resp).toJson(QJsonDocument::Compact);
#if QT_VERSION >= 0x050400
		qDebug().noquote() << "<" << data;
#else
		qDebug() << "<" << data;
#endif
		QNetworkReply *reply = net->post(d->request, data);
		QTimer *timer = new QTimer(this);
		timer->setSingleShot(true);
		connect(timer, &QTimer::timeout, reply, [=]{
			d->label->setText(tr("Request timed out"));
			d->close->show();
		});
		connect(timer, &QTimer::timeout, timer, &QTimer::deleteLater);
		timer->start(30*1000);
	}, Qt::QueuedConnection);
	connect(net, &QNetworkAccessManager::finished, this, [=](QNetworkReply *reply){
		switch(reply->error())
		{
		case QNetworkReply::NoError:
			if(reply->header(QNetworkRequest::ContentTypeHeader) == "application/json")
			{
				QByteArray data = reply->readAll();
				delete reply;
				process(data);
				return;
			}
			else
			{
				d->label->setText("<b><font color=\"red\">" + tr("Invalid content type") + "</font></b>");
				d->close->show();
			}
			break;
		case QNetworkReply::TimeoutError:
		case QNetworkReply::HostNotFoundError:
		case QNetworkReply::UnknownNetworkError:
			d->label->setText("<b><font color=\"red\">" + tr("Updating certificates has failed. Check your internet connection and try again.") + "</font></b>");
			d->close->show();
			break;
		case QNetworkReply::SslHandshakeFailedError:
			d->label->setText("<b><font color=\"red\">" + tr("SSL handshake failed. Please restart the update process.") + "</font></b>");
			d->close->show();
			break;
		default:
			switch(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt())
			{
			case 503:
			case 509:
				d->label->setText("<b>" + tr("Updating certificates has failed. The server is overloaded, try again later.") + "</b>");
				break;
			default:
				d->label->setText("<b><font color=\"red\">" + reply->errorString() + "</font></b>");
			}
			d->close->show();
		}
		reply->deleteLater();
	}, Qt::QueuedConnection);

	Q_EMIT start();
	return QDialog::exec();
}

void Updater::run()
{
	if(!d->reader)
		return;

	SslCertificate c(d->cert);
	bool result = d->reader->connect() &&
		d->verifyPIN(c.toString( c.showCN() ? "CN serialNumber" : "GN SN serialNumber" ), 1).resultOk();
	d->reader->disconnect();
	if(!result)
		return accept();

	Q_EMIT send({
		{"cmd", "START"},
		{"lang", Settings().language()},
		{"platform", qApp->applicationOs()},
		{"version", qApp->applicationVersion()}
	});
}

int Updater::execute()
{ 
	Overlay overlay(parentWidget());
	overlay.show();
	auto rc = exec();
	overlay.close();

	return rc;
}
