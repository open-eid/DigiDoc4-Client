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

#include <QtCore/QStringList>
#include <QtNetwork/QSslCertificate>

class TokenData::Private: public QSharedData
{
public:
	QString card;
	QStringList cards, readers;
	QSslCertificate cert;
	TokenData::TokenFlags flags = nullptr;
};



TokenData::TokenData(): d( new Private ) {}
TokenData::TokenData(const TokenData &other) = default;
TokenData::~TokenData() = default;

QString TokenData::card() const { return d->card; }
void TokenData::setCard( const QString &card ) { d->card = card; }

QStringList TokenData::cards() const { return d->cards; }
void TokenData::setCards( const QStringList &cards ) { d->cards = cards; }

bool TokenData::cardsOrder( const QString &s1, const QString &s2 )
{
	auto cardsOrderScore = []( QChar c ) -> quint8 {
		switch( c.toLatin1() )
		{
		case 'N': return 6;
		case 'A': return 5;
		case 'P': return 4;
		case 'E': return 3;
		case 'F': return 2;
		case 'B': return 1;
		default: return 0;
		}
	};
	QRegExp r("(\\w{1,2})(\\d{7})");
	if( r.indexIn( s1 ) == -1 )
		return false;
	QStringList cap1 = r.capturedTexts();
	if( r.indexIn( s2 ) == -1 )
		return false;
	QStringList cap2 = r.capturedTexts();
	// new cards to front
	if( cap1[1].size() != cap2[1].size() )
		return cap1[1].size() > cap2[1].size();
	// card type order
	if( cap1[1][0] != cap2[1][0] )
		return cardsOrderScore( cap1[1][0] ) > cardsOrderScore( cap2[1][0] );
	// card version order
	if( cap1[1].size() > 1 && cap2[1].size() > 1 && cap1[1][1] != cap2[1][1] )
		return cap1[1][1] > cap2[1][1];
	// serial number order
	return cap1[2].toUInt() > cap2[2].toUInt();
}

QSslCertificate TokenData::cert() const { return d->cert; }
void TokenData::setCert( const QSslCertificate &cert ) { d->cert = cert; }

void TokenData::clear() { d = new Private; }

TokenData::TokenFlags TokenData::flags() const { return d->flags; }
void TokenData::setFlag( TokenFlags flag, bool enabled )
{
	if( enabled ) d->flags |= flag;
	else d->flags &= ~flag;
}
void TokenData::setFlags( TokenFlags flags ) { d->flags = flags; }

QStringList TokenData::readers() const { return d->readers; }
void TokenData::setReaders( const QStringList &readers ) { d->readers = readers; }

TokenData& TokenData::operator =( const TokenData &other ) = default;

bool TokenData::operator !=( const TokenData &other ) const { return !(operator==(other)); }

bool TokenData::operator ==( const TokenData &other ) const
{
	return d == other.d ||
		( d->card == other.d->card &&
		  d->cards == other.d->cards &&
		  d->readers == other.d->readers &&
		  d->cert == other.d->cert &&
		  d->flags == other.d->flags );
}
