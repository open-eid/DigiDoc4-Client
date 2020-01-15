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

#pragma once

#include <QtCore/QSharedDataPointer>

class QSslCertificate;
class TokenData
{
public:
	TokenData();
	TokenData( const TokenData &other );
	~TokenData();

	QString card() const;
	void setCard( const QString &card );

	QStringList cards() const;
	void setCards( const QStringList &cards );

	QSslCertificate cert() const;
	void setCert( const QSslCertificate &cert );

	void clear();

	TokenData& operator =( const TokenData &other );
	bool operator !=( const TokenData &other ) const;
	bool operator ==( const TokenData &other ) const;

private:
	class Private;
	QSharedDataPointer<Private> d;
};
