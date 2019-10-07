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

class SslCertificate: public QSslCertificate
{
	Q_DECLARE_TR_FUNCTIONS( SslCertificate )

public:
	enum EnhancedKeyUsage
	{
		EnhancedKeyUsageNone = -1,
		All = 0,
		ClientAuth,
		ServerAuth,
		EmailProtect,
		OCSPSign,
		TimeStamping
	};
	enum KeyUsage
	{
		KeyUsageNone = -1,
		DigitalSignature = 0,
		NonRepudiation,
		KeyEncipherment,
		DataEncipherment,
		KeyAgreement,
		KeyCertificateSign,
		CRLSign,
		EncipherOnly,
		DecipherOnly
	};
	enum CertType
	{
		UnknownType = 0,
		DigiIDType = 1 << 0,
		EstEidType = 1 << 1,
		MobileIDType = 1 << 2,
		OCSPType = 1 << 3,
		TempelType = 1 << 4,
		EidType = 1 << 5,

		TestType = 1 << 7,
		DigiIDTestType = DigiIDType|TestType,
		EstEidTestType = EstEidType|TestType,
		MobileIDTestType = MobileIDType|TestType,
		OCSPTestType = OCSPType|TestType,
		TempelTestType = TempelType|TestType
	};

	SslCertificate();
	SslCertificate( const QByteArray &data, QSsl::EncodingFormat format );
	SslCertificate( const QSslCertificate &cert );

	QString		issuerInfo( const QByteArray &tag ) const;
	QString		issuerInfo( QSslCertificate::SubjectInfo subject ) const;
	QString		subjectInfo( const QByteArray &tag ) const;
	QString		subjectInfo( QSslCertificate::SubjectInfo subject ) const;

	QByteArray	authorityKeyIdentifier() const;
	QHash<EnhancedKeyUsage,QString> enhancedKeyUsage() const;
	QString		friendlyName() const;
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
	QString		publicKeyHex() const;
	bool		showCN() const;
	QByteArray	serialNumber( bool hex = false ) const;
	QString		signatureAlgorithm() const;
	QByteArray	subjectKeyIdentifier() const;
	static QByteArray	toHex( const QByteArray &in, QChar separator = ' ' );
	QString		toString( const QString &format ) const;
	CertType	type() const;

	static QSslCertificate fromX509( Qt::HANDLE x509 );
	static QSslKey keyFromEVP( Qt::HANDLE evp );

private:
	Qt::HANDLE extension( int nid ) const;
};

uint qHash( const SslCertificate &cert );

class PKCS12Certificate
{
public:
	enum ErrorType
	{
		NullError = 0,
		InvalidPasswordError = 1,
		FileNotExist = 2,
		FailedToRead = 3,
		UnknownError = -1
	};
	PKCS12Certificate( QIODevice *device, const QString &pin );
	PKCS12Certificate( const QByteArray &data, const QString &pin );
	PKCS12Certificate( const PKCS12Certificate &other );
	~PKCS12Certificate();

	QList<QSslCertificate> caCertificates() const;
	QSslCertificate certificate() const;
	ErrorType error() const;
	QString errorString() const;
	bool isNull() const;
	QSslKey	key() const;

	static PKCS12Certificate fromPath( const QString &path, const QString &pin );

private:
	class Private;
	QSharedDataPointer<Private> d;
};
