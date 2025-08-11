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
	enum EnhancedKeyUsage : quint8
	{
		All,
		ClientAuth,
		ServerAuth,
		EmailProtect,
		OCSPSign,
		TimeStamping
	};
	enum KeyUsage : quint8
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
	enum AuthorityInfoAccess : quint8
	{
		ad_OCSP,
		ad_CAIssuers,
	};
	enum CertType : quint8
	{
		UnknownType = 0,
		DigiIDType = 1 << 0,
		EstEidType = 1 << 1,
		MobileIDType = 1 << 2,
		OldEstEidType = 1 << 3,
		TempelType = 1 << 4,
		EResidentSubType = 1 << 6,
		EResidentType = DigiIDType|EResidentSubType,
	};
	enum Validity : quint8 {
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
	bool isValid() const {
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
	static QByteArray	toHex(const QByteArray &in, char separator = ' ');
	QString		toString( const QString &format ) const;
	CertType	type() const;
	Validity	validateOnline() const;

private:
	template<auto D>
	auto extension(int nid) const;
};

size_t qHash(const SslCertificate &cert);
