// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QSigner.h"

#include "Application.h"
#include "QCryptoBackend.h"
#include "QSmartCard.h"
#include "TokenData.h"
#include "Utils.h"

#include <digidocpp/Conf.h>
#include <digidocpp/crypto/Signer.h>
#include <digidocpp/crypto/X509Cert.h>

#include <QtNetwork/QSslCertificate>

using namespace digidoc;

QSigner::QSigner(const TokenData &token)
	: m_token(token)
{
	if(m_token.data(QStringLiteral("PSS")).toBool())
	{
		std::string method;
		switch(methodToNID(CONF(signatureDigestUri)))
		{
		case QCryptographicHash::Sha224: method = "http://www.w3.org/2007/05/xmldsig-more#sha224-rsa-MGF1"; break;
		case QCryptographicHash::Sha256: method = "http://www.w3.org/2007/05/xmldsig-more#sha256-rsa-MGF1"; break;
		case QCryptographicHash::Sha384: method = "http://www.w3.org/2007/05/xmldsig-more#sha384-rsa-MGF1"; break;
		case QCryptographicHash::Sha512: method = "http://www.w3.org/2007/05/xmldsig-more#sha512-rsa-MGF1"; break;
		default: break;
		}
		setMethod(method);
	}
}

X509Cert QSigner::cert() const
{
	if(m_token.cert().isNull())
		throw Exception(__FILE__, __LINE__, tr("Sign certificate is not selected").toStdString());
	QByteArray der = m_token.cert().toDer();
	return X509Cert((const unsigned char*)der.constData(), size_t(der.size()), X509Cert::Der);
}

QCryptographicHash::Algorithm QSigner::methodToNID(const std::string &method)
{
	if(method == "http://www.w3.org/2001/04/xmldsig-more#sha224" ||
		method == "http://www.w3.org/2001/04/xmldsig-more#rsa-sha224" ||
		method == "http://www.w3.org/2007/05/xmldsig-more#sha224-rsa-MGF1" ||
		method == "http://www.w3.org/2001/04/xmldsig-more#ecdsa-sha224") return QCryptographicHash::Sha224;
	if(method == "http://www.w3.org/2001/04/xmlenc#sha256" ||
		method == "http://www.w3.org/2001/04/xmldsig-more#rsa-sha256" ||
		method == "http://www.w3.org/2007/05/xmldsig-more#sha256-rsa-MGF1" ||
		method == "http://www.w3.org/2001/04/xmldsig-more#ecdsa-sha256") return QCryptographicHash::Sha256;
	if(method == "http://www.w3.org/2001/04/xmldsig-more#sha384" ||
		method == "http://www.w3.org/2001/04/xmldsig-more#rsa-sha384" ||
		method == "http://www.w3.org/2007/05/xmldsig-more#sha384-rsa-MGF1" ||
		method == "http://www.w3.org/2001/04/xmldsig-more#ecdsa-sha384") return QCryptographicHash::Sha384;
	if(method == "http://www.w3.org/2001/04/xmlenc#sha512" ||
		method == "http://www.w3.org/2001/04/xmldsig-more#rsa-sha512" ||
		method == "http://www.w3.org/2007/05/xmldsig-more#sha512-rsa-MGF1" ||
		method == "http://www.w3.org/2001/04/xmldsig-more#ecdsa-sha512") return QCryptographicHash::Sha512;
	return QCryptographicHash::Sha256;
}

std::vector<unsigned char> QSigner::sign(const std::string &method, const std::vector<unsigned char> &digest) const
{
	#define throwException(msg, code) { \
		Exception e(__FILE__, __LINE__, (msg).toStdString()); \
		e.setCode(code); \
		throw e; \
	}

	auto val = QCryptoBackend::getBackend(m_token);
	if(!val)
	{
		auto err = tr("Failed to login token") + ' ' +
			QCryptoBackend::errorString(val.error());
		switch(val.error()) {
		case QCryptoBackend::PinCanceled: throwException(err, Exception::PINCanceled);
		case QCryptoBackend::PinLocked:   throwException(err, Exception::PINLocked);
		case QCryptoBackend::InProgress:  throwException(err, Exception::General);
		default:                          throwException(err, Exception::PINFailed);
		}
	}
	std::unique_ptr<QCryptoBackend> backend(val.value());

	QByteArray sig = waitFor(&QCryptoBackend::sign, backend.get(),
		methodToNID(method), QByteArray::fromRawData((const char*)digest.data(), int(digest.size())));
	if(sig.isEmpty())
	{
		auto err = tr("Failed to login token") + ' ' +
			QCryptoBackend::errorString(backend->status);
		switch(backend->status) {
		case QCryptoBackend::PinCanceled: throwException(err, Exception::PINCanceled);
		case QCryptoBackend::PinLocked:   throwException(err, Exception::PINLocked);
		default: break;
		}
		throwException(tr("Failed to sign document"), Exception::General);
	}
	return {sig.constBegin(), sig.constEnd()};
}
