// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "TokenData.h"

#include <QtNetwork/QSslCertificate>
#include <QtCore/QVariantHash>

class TokenData::Private: public QSharedData
{
public:
	QString card, reader;
	QSslCertificate cert;
	QVariantHash data;
};



TokenData::TokenData(): d( new Private ) {}
TokenData::TokenData(const TokenData &other) = default;
TokenData::TokenData(TokenData &&other) Q_DECL_NOEXCEPT = default;
TokenData::~TokenData() = default;

QString TokenData::card() const { return d->card; }
void TokenData::setCard( const QString &card ) { d->card = card; }

QSslCertificate TokenData::cert() const { return d->cert; }
void TokenData::setCert( const QSslCertificate &cert ) { d->cert = cert; }

void TokenData::clear() { d = new Private; }

QString TokenData::reader() const { return d->reader; }
void TokenData::setReader(const QString &reader) { d->reader = reader; }

QVariant TokenData::data(const QString &key) const { return d->data.value(key); }
void TokenData::setData(const QString &key, const QVariant &value) { d->data[key] = value; }

bool TokenData::isNull() const {
	return d->card.isEmpty() && d->cert.isNull();
}

TokenData& TokenData::operator =( const TokenData &other ) = default;
TokenData& TokenData::operator =(TokenData &&other) Q_DECL_NOEXCEPT = default;

bool TokenData::operator ==( const TokenData &other ) const
{
	return d == other.d || (
		d->card == other.d->card &&
		d->reader == other.d->reader &&
		d->cert == other.d->cert);
}
