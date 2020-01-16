/*
 * QDigiDocCommon
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

#include "TokenData.h"

#include <QtNetwork/QSslCertificate>

class TokenData::Private: public QSharedData
{
public:
	QString card;
	QSslCertificate cert;
};



TokenData::TokenData(): d( new Private ) {}
TokenData::TokenData(const TokenData &other) = default;
TokenData::~TokenData() = default;

QString TokenData::card() const { return d->card; }
void TokenData::setCard( const QString &card ) { d->card = card; }

QSslCertificate TokenData::cert() const { return d->cert; }
void TokenData::setCert( const QSslCertificate &cert ) { d->cert = cert; }

void TokenData::clear() { d = new Private; }

TokenData& TokenData::operator =( const TokenData &other ) = default;

bool TokenData::operator !=( const TokenData &other ) const { return !(operator==(other)); }

bool TokenData::operator ==( const TokenData &other ) const
{
	return d == other.d || (d->card == other.d->card && d->cert == other.d->cert);
}
