// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

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
		TempelType = 1 << 4,
		EResidentSubType = 1 << 6,
		EResidentType = DigiIDType|EResidentSubType,
	};
	enum Validity : quint8 {
		Good = 0,
		Revoked = 1,
		Unknown = 2,
		Invalid = 3,
		Error = 4,
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
