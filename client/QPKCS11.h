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

#include <QtCore/QObject>

#include <common/SslCertificate.h>

class TokenData;
class QPKCS11Private;
class QPKCS11StackPrivate;

class QPKCS11: public QObject
{
	Q_OBJECT
public:
	enum PinStatus
	{
		PinOK,
		PinCanceled,
		PinIncorrect,
		PinLocked,
		DeviceError,
		GeneralError,
		UnknownError
	};

	explicit QPKCS11( QObject *parent = 0 );
	~QPKCS11();

	QByteArray encrypt( const QByteArray &data ) const;
	QByteArray decrypt( const QByteArray &data ) const;
	QByteArray derive(const QByteArray &publicKey) const;
	QByteArray deriveConcatKDF(const QByteArray &publicKey, const QString &digest, int keySize,
		const QByteArray &algorithmID, const QByteArray &partyUInfo, const QByteArray &partyVInfo) const;
	bool isLoaded() const;
	bool load( const QString &driver );
	PinStatus login( const TokenData &t );
	void logout();
	QStringList readers() const;
	QByteArray sign( int type, const QByteArray &digest ) const;
	QList<TokenData> tokens() const;
	bool verify( const QByteArray &data, const QByteArray &signature ) const;

	static QString errorString( PinStatus error );
private:
	QPKCS11Private *d;
};

class QPKCS11Stack: public QObject
{
	Q_OBJECT
public:
	explicit QPKCS11Stack( QObject *parent = 0 );
	~QPKCS11Stack();

	QByteArray encrypt( const QByteArray &data ) const;
	QByteArray decrypt( const QByteArray &data ) const;
	QByteArray derive(const QByteArray &publicKey) const;
	QByteArray deriveConcatKDF(const QByteArray &publicKey, const QString &digest, int keySize,
		const QByteArray &algorithmID, const QByteArray &partyUInfo, const QByteArray &partyVInfo) const;
	bool isLoaded() const;
	bool load( const QString &defaultDriver );
	QPKCS11::PinStatus login( const TokenData &t );
	void logout();
	QStringList readers() const;
	QByteArray sign( int type, const QByteArray &digest ) const;
	QList<TokenData> tokens() const;
	bool verify( const QByteArray &data, const QByteArray &signature ) const;

private:
	void updateDrivers() const;
	void loadDriver( const QString &driver ) const;
	QPKCS11StackPrivate *d;
};
