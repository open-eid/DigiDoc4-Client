// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QtCore/QSharedDataPointer>

class QSslCertificate;
class QVariant;
class TokenData
{
public:
	TokenData();
	TokenData( const TokenData &other );
	TokenData(TokenData &&other) Q_DECL_NOEXCEPT;
	~TokenData();

	QString card() const;
	void setCard( const QString &card );

	QSslCertificate cert() const;
	void setCert( const QSslCertificate &cert );

	QString reader() const;
	void setReader(const QString &reader);

	void clear();
	bool isNull() const;

	QVariant data(const QString &key) const;
	void setData(const QString &key, const QVariant &value);

	TokenData& operator =( const TokenData &other );
	TokenData& operator =(TokenData &&other) Q_DECL_NOEXCEPT;
	bool operator ==( const TokenData &other ) const;

private:
	class Private;
	QSharedDataPointer<Private> d;
};
