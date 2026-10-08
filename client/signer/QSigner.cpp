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

#include "QSigner.h"

#include "QCryptoBackend.h"
#include "TokenData.h"
#include "Utils.h"

#include <digidocpp/Conf.h>
#include <digidocpp/Exception.h>
#include <digidocpp/crypto/X509Cert.h>

#include <QtNetwork/QSslCertificate>

using namespace digidoc;

QSigner::QSigner(std::unique_ptr<QCryptoBackend> backend)
	: m_backend(std::move(backend))
{
	if(waitFor(&QCryptoBackend::isPSS, m_backend.get()))
	{
		std::string method;
		switch(methodToParams(CONF(signatureDigestUri)).algo)
		{
		case QCryptographicHash::Sha256: method = "http://www.w3.org/2007/05/xmldsig-more#sha256-rsa-MGF1"; break;
		case QCryptographicHash::Sha384: method = "http://www.w3.org/2007/05/xmldsig-more#sha384-rsa-MGF1"; break;
		case QCryptographicHash::Sha512: method = "http://www.w3.org/2007/05/xmldsig-more#sha512-rsa-MGF1"; break;
		default: break;
		}
		setMethod(method);
	}
}

QSigner::~QSigner() noexcept = default;

X509Cert QSigner::cert() const
{
	if(m_backend->cert().isNull())
		throw Exception(__FILE__, __LINE__, tr("Sign certificate is not selected").toStdString());
	QByteArray der = m_backend->cert().toDer();
	return X509Cert((const unsigned char*)der.constData(), size_t(der.size()), X509Cert::Der);
}

QSigner::Params QSigner::methodToParams(const std::string &method)
{
	if(method == "http://www.w3.org/2001/04/xmlenc#sha256" ||
		method == "http://www.w3.org/2001/04/xmldsig-more#ecdsa-sha256")
		return {QCryptographicHash::Sha256, QCryptoBackend::NoPadding};
	if(method == "http://www.w3.org/2001/04/xmldsig-more#rsa-sha256")
		return {QCryptographicHash::Sha256, QCryptoBackend::PKCS1Padding};
	if(method == "http://www.w3.org/2007/05/xmldsig-more#sha256-rsa-MGF1")
		return {QCryptographicHash::Sha256, QCryptoBackend::PSSPadding};
	if(method == "http://www.w3.org/2001/04/xmldsig-more#sha384" ||
		method == "http://www.w3.org/2001/04/xmldsig-more#ecdsa-sha384")
		return {QCryptographicHash::Sha384, QCryptoBackend::NoPadding};
	if(method == "http://www.w3.org/2001/04/xmldsig-more#rsa-sha384")
		return {QCryptographicHash::Sha384, QCryptoBackend::PKCS1Padding};
	if(method == "http://www.w3.org/2007/05/xmldsig-more#sha384-rsa-MGF1")
		return {QCryptographicHash::Sha384, QCryptoBackend::PSSPadding};
	if(method == "http://www.w3.org/2001/04/xmlenc#sha512" ||
		method == "http://www.w3.org/2001/04/xmldsig-more#ecdsa-sha512")
		return {QCryptographicHash::Sha512, QCryptoBackend::NoPadding};
	if(method == "http://www.w3.org/2001/04/xmldsig-more#rsa-sha512")
		return {QCryptographicHash::Sha512, QCryptoBackend::PKCS1Padding};
	if(method == "http://www.w3.org/2007/05/xmldsig-more#sha512-rsa-MGF1")
		return {QCryptographicHash::Sha512, QCryptoBackend::PSSPadding};
	return {QCryptographicHash::Sha256, QCryptoBackend::NoPadding};
}

std::vector<unsigned char> QSigner::sign(const std::string &method, const std::vector<unsigned char> &digest) const
{
	#define throwException(msg, code) { \
		Exception e(__FILE__, __LINE__, (msg).toStdString()); \
		e.setCode(code); \
		throw e; \
	}

	const auto params = methodToParams(method);
	auto sig = waitFor(&QCryptoBackend::sign, m_backend.get(), params.algo, params.padding, digest);
	if(!sig.empty())
		return sig;
	auto err = tr("Failed to login token") + ' ' + QCryptoBackend::errorString(m_backend->status);
	switch(m_backend->status) {
	case QCryptoBackend::PinCanceled: throwException(err, Exception::PINCanceled);
	case QCryptoBackend::PinLocked:   throwException(err, Exception::PINLocked);
	default:
		throwException(tr("Failed to sign document"), Exception::General);
	}
}
