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

#pragma once

#include <QtNetwork/QSslCertificate>

#include <QtCore/QCoreApplication>

template<class Key,class T> class QHash;
template<class Key,class T> class QMultiHash;

class SslCertificate: public QSslCertificate
{
	Q_DECLARE_TR_FUNCTIONS( SslCertificate )

public:
	enum EnhancedKeyUsage
	{
		All,
		ClientAuth,
		ServerAuth,
		EmailProtect,
		OCSPSign,
		TimeStamping
	};
	enum KeyUsage
	{
		DigitalSignature,
		NonRepudiation,
		KeyEncipherment,
		DataEncipherment,
		KeyAgreement,
		KeyCertificateSign,
		CRLSign,
		EncipherOnly,
		DecipherOnly
	};
	enum AuthorityInfoAccess
	{
		ad_OCSP,
		ad_CAIssuers,
	};
	enum CertType
	{
		UnknownType = 0,
		DigiIDType = 1 << 0,
		EstEidType = 1 << 1,
		MobileIDType = 1 << 2,
		TempelType = 1 << 4,
		EResidentSubType = 1 << 6,
		EResidentType = DigiIDType|EResidentSubType,
	};
	enum Validity {
		Good = 0,
		Revoked = 1,
		Unknown = 2,
	};

	SslCertificate();
	SslCertificate( const QByteArray &data, QSsl::EncodingFormat format );
	SslCertificate( const QSslCertificate &cert );

	QString		issuerInfo( const QByteArray &tag ) const;
	QString		issuerInfo( QSslCertificate::SubjectInfo subject ) const;
	QString		subjectInfo( const QByteArray &tag ) const;
	QString		subjectInfo( QSslCertificate::SubjectInfo subject ) const;

	QMultiHash<AuthorityInfoAccess,QString> authorityInfoAccess() const;
	QByteArray	authorityKeyIdentifier() const;
	QHash<EnhancedKeyUsage,QString> enhancedKeyUsage() const;
	bool		isCA() const;
	inline bool isValid() const {
		const QDateTime currentTime = QDateTime::currentDateTime();
		return currentTime >= effectiveDate() &&
			currentTime <= expiryDate() &&
			!isBlacklisted();
	}
	QString		keyName() const;
	QHash<KeyUsage,QString> keyUsage() const;
	QString		personalCode() const;
	QStringList policies() const;
	bool		showCN() const;
	QString		signatureAlgorithm() const;
	QByteArray	subjectKeyIdentifier() const;
	static QByteArray	toHex( const QByteArray &in, QChar separator = ' ' );
	QString		toString( const QString &format ) const;
	CertType	type() const;
	Validity	validateOnline() const;

private:
	Qt::HANDLE extension( int nid ) const;
};

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
size_t qHash(const SslCertificate &cert);
#else
uint qHash(const SslCertificate &cert);
#endif
