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

#include <QObject>

#include <common/SslCertificate.h>

class QCSPPrivate;
class TokenData;

class QCSP: public QObject
{
	Q_OBJECT
public:
	typedef QHash<SslCertificate,QString> Certs;
	enum PinStatus
	{
		PinOK,
		PinCanceled,
		PinUnknown,
	};

	explicit QCSP( QObject *parent = 0 );
	~QCSP();

	Certs certs() const;
	QByteArray decrypt( const QByteArray &data );
	PinStatus lastError() const;
	TokenData selectCert( const SslCertificate &cert );
	QByteArray sign( int method, const QByteArray &digest );

private:
	QCSPPrivate *d;
};
