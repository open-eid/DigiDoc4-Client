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

#include "SslCertificate.h"

#include "Common.h"
#include "Crypto.h"

#include <digidocpp/Exception.h>
#include <digidocpp/crypto/X509Cert.h>

#include <QtCore/QDebug>
#include <QtCore/QRegularExpression>
#include <QtCore/QStringList>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QSslKey>

#include <openssl/ocsp.h>
#include <openssl/x509v3.h>

#include <memory>

template<auto D>
struct free_deleter
{
	template<class T>
	void operator()(T *p) const noexcept
	{
		D(p);
	}
};

template<typename> struct free_argument;
template<class T, class R>
struct free_argument<R (*)(T *)>
{
	using type = T;
};
template<class T, class R>
struct free_argument<R (&)(T *)>
{
	using type = T;
};

template<auto F, typename T>
constexpr auto make_unique_ptr(T *t)
{
	return std::unique_ptr<T, free_deleter<F>>(t);
}

template<class T>
static auto toQByteArray(T &x)
{
	return QByteArray((const char*)x->data, x->length);
}

template <typename Func, typename Arg>
static QByteArray i2dDer(Func func, Arg arg)
{
	if(!arg)
		return {};
	QByteArray der(func(arg, nullptr), 0);
	auto *p = (unsigned char*)der.data();
	if(der.isEmpty() || func(arg, &p) != der.size())
		return {};
	return der;
}

size_t qHash(const SslCertificate &cert) { return qHash(cert.digest()); }

SslCertificate::SslCertificate() = default;

SslCertificate::SslCertificate( const QByteArray &data, QSsl::EncodingFormat format )
: QSslCertificate( data, format ) {}

SslCertificate::SslCertificate( const QSslCertificate &cert )
: QSslCertificate( cert ) {}

QString SslCertificate::issuerInfo( const QByteArray &tag ) const
{ return QSslCertificate::issuerInfo(tag).join(' '); }

QString SslCertificate::issuerInfo( QSslCertificate::SubjectInfo subject ) const
 { return QSslCertificate::issuerInfo(subject).join(' '); }

QString SslCertificate::subjectInfo( const QByteArray &tag ) const
 { return QSslCertificate::subjectInfo(tag).join(' '); }

QString SslCertificate::subjectInfo( QSslCertificate::SubjectInfo subject ) const
{ return QSslCertificate::subjectInfo(subject).join(' '); }

template<auto D>
auto SslCertificate::extension(int nid) const
{
	using T = typename free_argument<decltype(D)>::type;
	return std::unique_ptr<T, free_deleter<D>>(static_cast<T*>(handle() ? X509_get_ext_d2i((X509*)handle(), nid, nullptr, nullptr) : nullptr));
}

QMultiHash<SslCertificate::AuthorityInfoAccess, QString> SslCertificate::authorityInfoAccess() const
{
	QMultiHash<AuthorityInfoAccess, QString> result;
	auto info = extension<AUTHORITY_INFO_ACCESS_free>(NID_info_access);
	if(!info)
		return result;
	for(int i = 0; i < sk_ACCESS_DESCRIPTION_num(info.get()); ++i)
	{
		ACCESS_DESCRIPTION *ad = sk_ACCESS_DESCRIPTION_value(info.get(), i);
		if(ad->location->type != GEN_URI)
			continue;
		switch(OBJ_obj2nid(ad->method))
		{
		case NID_ad_OCSP:
			result.insert(ad_OCSP, toQByteArray(ad->location->d.uniformResourceIdentifier));
			break;
		case NID_ad_ca_issuers:
			result.insert(ad_CAIssuers, toQByteArray(ad->location->d.uniformResourceIdentifier));
			break;
		default: break;
		}
	}
	return result;
}

QByteArray SslCertificate::authorityKeyIdentifier() const
{
	auto id = extension<AUTHORITY_KEYID_free>(NID_authority_key_identifier);
	return id && id->keyid ? toQByteArray(id->keyid) : QByteArray();
}

QHash<SslCertificate::EnhancedKeyUsage,QString> SslCertificate::enhancedKeyUsage() const
{
	auto usage = extension<EXTENDED_KEY_USAGE_free>(NID_ext_key_usage);
	if(!usage)
		return { {All, tr("All application policies")} };

	QHash<EnhancedKeyUsage,QString> list;
	for(int i = 0; i < sk_ASN1_OBJECT_num(usage.get()); ++i)
	{
		ASN1_OBJECT *obj = sk_ASN1_OBJECT_value(usage.get(), i);
		switch( OBJ_obj2nid( obj ) )
		{
		case NID_client_auth:
			list[ClientAuth] = tr("Proves your identity to a remote computer"); break;
		case NID_server_auth:
			list[ServerAuth] = tr("Ensures the identity of a remote computer"); break;
		case NID_email_protect:
			list[EmailProtect] = tr("Protects email messages"); break;
		case NID_OCSP_sign:
			list[OCSPSign] = tr("OCSP signing"); break;
		case NID_time_stamp:
			list[TimeStamping] = tr("Time Stamping"); break;
		default: break;
		}
	}
	return list;
}

bool SslCertificate::isCA() const
{
	auto cons = extension<BASIC_CONSTRAINTS_free>(NID_basic_constraints);
	return cons && cons->ca > 0;
}

QString SslCertificate::keyName() const
{
	switch(publicKey().algorithm())
	{
	case QSsl::Dsa:
		return QStringLiteral("DSA (%1)").arg( publicKey().length() );
	case QSsl::Rsa:
		return QStringLiteral("RSA (%1)").arg( publicKey().length() );
	default:
#ifndef OPENSSL_NO_ECDSA
		if(X509 *c = (X509*)handle())
			return Crypto::curve_oid(X509_get0_pubkey(c));
#endif
	}
	return tr("Unknown");
}

QHash<SslCertificate::KeyUsage,QString> SslCertificate::keyUsage() const
{
	QHash<KeyUsage,QString> list;
	auto keyusage = extension<ASN1_BIT_STRING_free>(NID_key_usage);
	if(!keyusage)
		return list;
	for( int n = 0; n < 9; ++n )
	{
		if(!ASN1_BIT_STRING_get_bit(keyusage.get(), n))
			continue;
		switch( n )
		{
		case DigitalSignature: list[KeyUsage(n)] = tr("Digital signature"); break;
		case NonRepudiation: list[KeyUsage(n)] = tr("Non repudiation"); break;
		case KeyEncipherment: list[KeyUsage(n)] = tr("Key encipherment"); break;
		case DataEncipherment: list[KeyUsage(n)] = tr("Data encipherment"); break;
		case KeyAgreement: list[KeyUsage(n)] = tr("Key agreement"); break;
		case KeyCertificateSign: list[KeyUsage(n)] = tr("Key certificate sign"); break;
		case CRLSign: list[KeyUsage(n)] = tr("CRL sign"); break;
		case EncipherOnly: list[KeyUsage(n)] = tr("Encipher only"); break;
		case DecipherOnly: list[KeyUsage(n)] = tr("Decipher only"); break;
		default: break;
		}
	}
	return list;
}

QString SslCertificate::personalCode() const
{
	// http://www.etsi.org/deliver/etsi_en/319400_319499/31941201/01.01.01_60/en_31941201v010101p.pdf
	static const QStringList types {"PAS", "IDC", "PNO", "TAX", "TIN"};
	QString data = subjectInfo(QSslCertificate::SerialNumber);
	if(data.size() > 6 && (types.contains(data.left(3)) || data[2] == ':') && data[5] == '-')
		return data.mid(6);
	if(!data.isEmpty())
		return data;
	return QString(serialNumber()).remove(':');
}

QStringList SslCertificate::policies() const
{
	QStringList list;
	auto cp = extension<CERTIFICATEPOLICIES_free>(NID_certificate_policies);
	if( !cp )
		return list;

	for(int i = 0; i < sk_POLICYINFO_num(cp.get()); ++i)
	{
		POLICYINFO *pi = sk_POLICYINFO_value(cp.get(), i);
		QByteArray buf(50, 0);
		int len = OBJ_obj2txt(buf.data(), buf.size(), pi->policyid, 1);
		if( len != NID_undef )
			list.append(buf);
	}
	return list;
}

bool SslCertificate::showCN() const
{ return subjectInfo( "GN" ).isEmpty() && subjectInfo( "SN" ).isEmpty(); }

QString SslCertificate::signatureAlgorithm() const
{
	if(!handle())
		return {};
	const X509_ALGOR *algo = nullptr;
	X509_get0_signature(nullptr, &algo, (X509*)handle());
	QByteArray buf(50, 0);
	i2t_ASN1_OBJECT(buf.data(), buf.size(), algo->algorithm);
	return buf;
}

QByteArray SslCertificate::subjectKeyIdentifier() const
{
	auto id = extension<ASN1_OCTET_STRING_free>(NID_subject_key_identifier);
	return !id ? QByteArray() : toQByteArray(id);
}

QByteArray SslCertificate::toHex(const QByteArray &in, char separator)
{
	return in.toHex(separator).toUpper();
}

QString SslCertificate::toString( const QString &format ) const
{
	static const QRegularExpression r(QStringLiteral("[a-zA-Z]+"));
	QString ret = format;
	QRegularExpressionMatch match;
	for(int pos = 0; (match = r.match(ret, pos)).hasMatch(); ) {
		QString cap = match.captured();
		QString si = cap == QLatin1String("serialNumber") ? personalCode() : subjectInfo(cap.toLatin1());
		ret.replace(match.capturedStart(), cap.size(), si);
		pos = match.capturedStart() + si.size();
	}
	return ret;
}

SslCertificate::CertType SslCertificate::type() const
{
	// https://www.id.ee/wp-content/uploads/2022/02/cp_esteid_01.10.2018_version1.0.pdf
	for(QString p: policies())
	{
		p.remove(QLatin1String("2.999."));
		if(p.startsWith(QLatin1String("1.3.6.1.4.1.10015.1.3")) ||
			p.startsWith(QLatin1String("1.3.6.1.4.1.10015.11.1")) ||
			p.startsWith(QLatin1String("1.3.6.1.4.1.10015.3.3")) ||
			p.startsWith(QLatin1String("1.3.6.1.4.1.10015.3.11")))
			return MobileIDType;
		if(p.startsWith(QLatin1String("1.3.6.1.4.1.10015.7.1")) ||
			p.startsWith(QLatin1String("1.3.6.1.4.1.10015.7.3")) ||
			p.startsWith(QLatin1String("1.3.6.1.4.1.10015.2.1")) ||
			p.startsWith(QLatin1String("1.3.6.1.4.1.10015.3.7")))
			return TempelType;
		if(p.startsWith(QLatin1String("1.3.6.1.4.1.51361.1.1.3")) ||
			p.startsWith(QLatin1String("1.3.6.1.4.1.51361.1.2.3")))
			return DigiIDType;
		if(p.startsWith(QLatin1String("1.3.6.1.4.1.51361.1.1.4")) ||
			p.startsWith(QLatin1String("1.3.6.1.4.1.51361.1.2.4")) ||
			p.startsWith(QLatin1String("1.3.6.1.4.1.51361.2.1.6")))
			return EResidentType;
		if(p.startsWith(QLatin1String("1.3.6.1.4.1.51361.1.1")) ||
			p.startsWith(QLatin1String("1.3.6.1.4.1.51361.1.2")) ||
			p.startsWith(QLatin1String("1.3.6.1.4.1.51361.2.1")) ||
			p.startsWith(QLatin1String("1.3.6.1.4.1.51455.1.1")) ||
			p.startsWith(QLatin1String("1.3.6.1.4.1.51455.1.2")) ||
			p.startsWith(QLatin1String("1.3.6.1.4.1.51455.2.1")))
			return EstEidType;
	}

	// Check qcStatements extension according to ETSI EN 319 412-5
	if(QByteArray der = toDer(); !der.isNull())
	{
		try {
			digidoc::X509Cert x509Cert((const unsigned char*)der.constData(),
									   size_t(der.size()), digidoc::X509Cert::Der);
			for(const std::string &statement: x509Cert.qcStatements())
			{
				if(statement == digidoc::X509Cert::QCT_ESEAL)
					return TempelType;
			}
		} catch (const digidoc::Exception &e) {
			qWarning() << "digidoc::X509Cert error:" << QString::fromStdString(e.msg());
		}
	}
	return UnknownType;
}

SslCertificate::Validity SslCertificate::validateOnline() const
{
	QMultiHash<SslCertificate::AuthorityInfoAccess,QString> urls = authorityInfoAccess();
	if(urls.isEmpty())
		return Error;

	QEventLoop e;
	QNetworkAccessManager m;
	QNetworkAccessManager::connect(&m, &QNetworkAccessManager::finished, &e, &QEventLoop::quit);
	QNetworkAccessManager::connect(&m, &QNetworkAccessManager::sslErrors, &m,
		[](QNetworkReply *reply, const QList<QSslError> &errors){
		reply->ignoreSslErrors(errors);
	});

	// Get issuer
	QNetworkRequest r(urls.values(SslCertificate::ad_CAIssuers).first());
	r.setRawHeader("User-Agent", QStringLiteral("%1/%2 (%3)")
		.arg(QCoreApplication::applicationName(), QCoreApplication::applicationVersion(), Common::applicationOs()).toUtf8());
	QNetworkReply *repl = m.get(r);
	e.exec();
	QSslCertificate issuer(repl->readAll(), QSsl::Der);
	repl->deleteLater();
	if(issuer.isNull())
		return Error;

	// Build request
	auto ocspReq = make_unique_ptr<OCSP_REQUEST_free>(OCSP_REQUEST_new());
	if(!ocspReq)
		return Error;
	OCSP_CERTID *certId = OCSP_cert_to_id(nullptr, (X509*)handle(), (X509*)issuer.handle());
	if(!OCSP_request_add0_id(ocspReq.get(), certId))
		return Error;

	// Send request
	r.setUrl(urls.values(SslCertificate::ad_OCSP).first());
	r.setHeader(QNetworkRequest::ContentTypeHeader, "application/ocsp-request");
	repl = m.post(r, i2dDer(i2d_OCSP_REQUEST, ocspReq.get()));
	e.exec();

	// Parse response
	QByteArray respData = repl->readAll();
	repl->deleteLater();
	const unsigned char *p = (const unsigned char*)respData.constData();
	auto resp = make_unique_ptr<OCSP_RESPONSE_free>(d2i_OCSP_RESPONSE(nullptr, &p, respData.size()));
	if(!resp || OCSP_response_status(resp.get()) != OCSP_RESPONSE_STATUS_SUCCESSFUL)
		return Error;

	// Validate response
	auto basic = make_unique_ptr<OCSP_BASICRESP_free>(OCSP_response_get1_basic(resp.get()));
	if(!basic)
		return Error;
	if(OCSP_basic_verify(basic.get(), nullptr, nullptr, OCSP_NOVERIFY) <= 0)
		return Invalid;
	int status = -1;
	if(OCSP_resp_find_status(basic.get(), certId, &status, nullptr, nullptr, nullptr, nullptr) <= 0)
		return Unknown;
	return Validity(status);
}
