/*
 * QDigiDocCrypto
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

#include "LdapSearch.h"

#include "Application.h"
#include "TLV.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QJsonArray>
#include <QtCore/QTimer>
#include <QtCore/QVariantMap>
#include <QtNetwork/QNetworkProxy>
#include <QtNetwork/QSslCertificate>
#include <QtNetwork/QSslSocket>

#include <chrono>
#include <type_traits>
#include <utility>

using namespace std::chrono;

namespace {

// RFC 4511 BER tags
enum Tag : quint8 {
	BOOLEAN = 0x01,
	INTEGER = 0x02,
	OCTET_STRING = 0x04,
	ENUMERATED = 0x0A,
	SEQUENCE = 0x30,
	UNBIND_REQUEST = 0x42,
	SEARCH_REQUEST = 0x63,
	SEARCH_RESULT_ENTRY = 0x64,
	SEARCH_RESULT_DONE = 0x65,
	SEARCH_RESULT_REFERENCE = 0x73,
	FILTER_EQUALITY = 0xA3,
	FILTER_SUBSTRINGS = 0xA4,
	SUBSTRING_ANY = 0x81,
};

constexpr int MESSAGE_ID = 1;
constexpr int SIZE_LIMIT = 50;
constexpr int RESULT_SUCCESS = 0;
constexpr int RESULT_SIZE_LIMIT_EXCEEDED = 4;
constexpr qsizetype MAX_RESPONSE_SIZE = 10 * 1024 * 1024;

template<typename T>
struct Ber final
{
	Tag tag;
	T value;
	constexpr qsizetype encodedSize() const;
	operator QByteArray() const;
};

constexpr int lengthSize(qsizetype size)
{
	int length = 1;
	if(size >= 0x80)
		for(; size > 0; size >>= 8)
			++length;
	return length;
}

qsizetype payloadSize(const QByteArray &value)
{
	return value.size();
}

constexpr int payloadSize(int value)
{
	int length = 1;
	for(; value < -128 || value > 127; value >>= 8)
		++length;
	return length;
}

template<typename T>
constexpr qsizetype payloadSize(const Ber<T> &value)
{
	return value.encodedSize();
}

template<typename T>
constexpr qsizetype Ber<T>::encodedSize() const
{
	qsizetype size = payloadSize(value);
	return 1 + lengthSize(size) + size;
}

template<typename T>
inline QByteArray operator+(QByteArray lhs, const Ber<T>& rhs)
{
	// Tag
	lhs.append(char(rhs.tag));
	// Length
	auto size = payloadSize(rhs.value);
	if(int length = lengthSize(size); length == 1)
		lhs.append(char(size));
	else
	{
		lhs.append(char(0x80 | (length - 1)));
		for(int shift = (length - 2) * 8; shift >= 0; shift -= 8)
			lhs.append(char((size >> shift) & 0xFF));
	}
	// Value
	if constexpr(std::is_same_v<T, QByteArray>)
		lhs.append(rhs.value);
	else if constexpr(std::is_same_v<T, int>)
	{
		for(int shift = (size - 1) * 8; shift >= 0; shift -= 8)
			lhs.append(char((quint32(rhs.value) >> shift) & 0xFF));
	}
	else
		return std::move(lhs) + rhs.value;
	return lhs;
}

template<typename T>
Ber<T>::operator QByteArray() const
{
	QByteArray result;
	result.reserve(encodedSize());
	return std::move(result) + *this;
}

QByteArray ldapMessage(Tag operation, auto &&value)
{
	return Ber{SEQUENCE, Ber{INTEGER, MESSAGE_ID} + Ber{operation, std::forward<decltype(value)>(value)}};
}

int toInt(QByteArrayView data)
{
	if(data.isEmpty() || data.size() > qsizetype(sizeof(int)))
		return -1;
	int result = 0;
	for(char c: data)
		result = (result << 8) | quint8(c);
	return result;
}

struct Request final: public QSslSocket
{
	Q_DECLARE_TR_FUNCTIONS(LdapSearch)

public:
	Request(LdapSearch *owner, const QUrl &url, const QSslConfiguration &ssl,
		const QByteArray &request, const QVariantMap &userData)
		: QSslSocket(owner)
		, owner(owner)
		, host(url.host())
		, tls(url.scheme() == QLatin1String("ldaps"))
		, port(quint16(url.port(tls ? 636 : 389)))
		, userData(userData)
	{
		timer.setSingleShot(true);
		connect(&timer, &QTimer::timeout, this, &Request::timeout);
		connect(this, &QAbstractSocket::errorOccurred, this, &Request::socketError);
		connect(this, &QAbstractSocket::disconnected, this, &QObject::deleteLater);
		connect(this, &QIODevice::readyRead, this, &Request::readResults);
		if(tls)
			connect(this, &QSslSocket::encrypted, this, [this, request] { write(request); });
		else
			connect(this, &QAbstractSocket::connected, this, [this, request] { write(request); });
		setSslConfiguration(ssl);
		connectToServer();
	}

private:
	void connectToServer()
	{
		timer.start(30s);
		if(tls)
			connectToHostEncrypted(host, port);
		else
			connectToHost(host, port);
	}

	void fail(const QString &title, const QString &details = {})
	{
		finished = true;
		abort();
		deleteLater();
		Q_EMIT owner->error(title, details.isEmpty() ? host : QStringLiteral("%1\n%2").arg(host, details));
	}

	bool viaProxy() const
	{
		if(proxy().type() != QNetworkProxy::DefaultProxy)
			return proxy().type() != QNetworkProxy::NoProxy;
		// Same lookup that QAbstractSocket does for the default proxy
		for(const QNetworkProxy &p: QNetworkProxyFactory::proxyForQuery(QNetworkProxyQuery(host, port, protocolTag())))
		{
			if(p.capabilities() & QNetworkProxy::TunnelingCapability)
				return p.type() != QNetworkProxy::NoProxy;
		}
		return false;
	}

	// Proxies often allow CONNECT only to port 443, while firewalls allow
	// the direct LDAP connection that libldap and WinLDAP always used
	bool retryDirect()
	{
		if(!viaProxy())
			return false;
		abort();
		setProxy(QNetworkProxy::NoProxy);
		// Reconnecting from errorOccurred is not supported, retry from the event loop
		QTimer::singleShot(0, this, &Request::connectToServer);
		return true;
	}

	void timeout()
	{
		if(!finished)
		{
			// The proxy did not answer CONNECT
			if(state() != QAbstractSocket::ConnectedState && retryDirect())
				return;
			return fail(tr("Check your internet connection!\nLDAP server is unavailable."));
		}
		abort();
		deleteLater();
	}

	void socketError(QAbstractSocket::SocketError error)
	{
		if(finished)
			return;
		switch(error)
		{
		case QAbstractSocket::SocketAccessError: // proxy answered 403/405 to CONNECT
		case QAbstractSocket::ProxyAuthenticationRequiredError:
		case QAbstractSocket::ProxyConnectionRefusedError:
		case QAbstractSocket::ProxyConnectionClosedError:
		case QAbstractSocket::ProxyConnectionTimeoutError:
		case QAbstractSocket::ProxyNotFoundError:
		case QAbstractSocket::ProxyProtocolError:
			if(retryDirect())
				return;
			break;
		default:
			break;
		}
		if(const auto errors = sslHandshakeErrors();
			error == QAbstractSocket::SslHandshakeFailedError && !errors.isEmpty())
		{
			QStringList details;
			for(const QSslError &sslError: errors)
				details.append(sslError.errorString());
			return fail(tr("The LDAP server certificate is not trusted."), details.join(QLatin1Char('\n')));
		}
		fail(tr("Check your internet connection!\nLDAP server is unavailable."), errorString());
	}

	void readResults()
	{
		if(finished)
			return;
		buffer.append(readAll());
		if(buffer.size() > MAX_RESPONSE_SIZE)
			return fail(tr("Failed to get result"));
		// An incomplete message stays in the buffer until the rest arrives
		for(TLV message(buffer); message; message = TLV(buffer))
		{
			TLV id(message.value());
			TLV op = id.next();
			if(message.tag != SEQUENCE || id.tag != INTEGER || toInt(id.value()) != MESSAGE_ID || !op)
				return fail(tr("Failed to get result"));
			switch(op.tag)
			{
			case SEARCH_RESULT_ENTRY:
				readEntry(op.value());
				break;
			case SEARCH_RESULT_REFERENCE:
				return fail(tr("Failed to get result"),
					tr("The LDAP search could not be completed because the server returned a referral."));
			case SEARCH_RESULT_DONE:
				return complete(op.value());
			default:
				return fail(tr("Failed to get result"));
			}
			buffer.remove(0, message.data.data() - buffer.constData() + message.length);
		}
	}

	void readEntry(QByteArrayView entry)
	{
		TLV attributes = TLV(entry).next(); // skip objectName
		for(TLV attribute(attributes.value()); attribute; ++attribute)
		{
			TLV type(attribute.value());
			if(const QByteArray name = type.value().toByteArray();
				name.compare("userCertificate;binary", Qt::CaseInsensitive) != 0 &&
				name.compare("userCertificate", Qt::CaseInsensitive) != 0)
				continue;
			TLV values = type.next();
			for(TLV cert(values.value()); cert; ++cert)
				certs.append(QSslCertificate(cert.value().toByteArray(), QSsl::Der));
		}
		++count;
	}

	void complete(QByteArrayView result)
	{
		TLV resultCode(result);
		TLV diagnosticMessage = resultCode.next().next(); // skip matchedDN
		int code = resultCode.tag == ENUMERATED ? toInt(resultCode.value()) : -1;
		if(code != RESULT_SUCCESS && code != RESULT_SIZE_LIMIT_EXCEEDED)
			return fail(tr("Failed to get result"), tr("Error Code: %1 (%2)")
				.arg(code).arg(QString::fromUtf8(diagnosticMessage.value())));
		finished = true;
		write(ldapMessage(UNBIND_REQUEST, QByteArray{}));
		disconnectFromHost();
		timer.start(5s);
		Q_EMIT owner->searchResult(certs, count, userData);
	}

	LdapSearch *owner;
	const QString host;
	const bool tls;
	const quint16 port;
	const QVariantMap userData;
	QTimer timer;
	QByteArray buffer;
	QList<QSslCertificate> certs;
	int count = 0;
	bool finished = false;
};

}

LdapSearch::LdapSearch(QObject *parent)
	: QObject(parent)
{
	QList<QSslCertificate> caCerts;
	for(const auto &entry: Application::confValue(QLatin1String("LDAP-CERTS")).toArray())
	{
		if(QSslCertificate cert(QByteArray::fromBase64(entry.toString().toLatin1()), QSsl::Der); !cert.isNull())
			caCerts.append(cert);
	}
	if(!caCerts.isEmpty())
		ssl.setCaCertificates(caCerts);
}

void LdapSearch::search(const QUrl &url, const QString &attribute, const QString &value, Match match, const QVariantMap &userData)
{
	QString base = url.path().mid(1);
	QByteArray request = ldapMessage(SEARCH_REQUEST,
		Ber{OCTET_STRING, base.isEmpty() ? QByteArrayLiteral("c=EE") : base.toUtf8()} +
		Ber{ENUMERATED, 2} + // scope: wholeSubtree
		Ber{ENUMERATED, 0} + // derefAliases: neverDerefAliases
		Ber{INTEGER, SIZE_LIMIT} +
		Ber{INTEGER, 0} + // timeLimit: none
		Ber{BOOLEAN, 0} + // typesOnly: false
		(match == Contains ?
			Ber{FILTER_SUBSTRINGS, Ber{OCTET_STRING, attribute.toUtf8()} + Ber{SEQUENCE, Ber{SUBSTRING_ANY, value.toUtf8()}}} :
			Ber{FILTER_EQUALITY, Ber{OCTET_STRING, attribute.toUtf8()} + Ber{OCTET_STRING, value.toUtf8()}}
		) +
		Ber{SEQUENCE, Ber{OCTET_STRING, QByteArrayLiteral("userCertificate;binary")}}
	);
	new Request(this, url, ssl, request, userData);
}
