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

#include <QtCore/QThread>
#include <digidocpp/crypto/Signer.h>

class QSmartCard;
class QSslKey;
class TokenData;

class QSigner final: public QThread, public digidoc::Signer
{
	Q_OBJECT

public:
	enum ApiType
	{
		CNG,
		CAPI,
		PKCS11
	};
	enum ErrorCode
	{
		PinCanceled,
		PinIncorrect,
		PinLocked,
		DecryptFailed,
		DecryptOK
	};
	explicit QSigner(ApiType api, QObject *parent = nullptr);
	~QSigner() final;

	ApiType apiType() const;
	QSet<QString> cards() const;
	QList<TokenData> cache() const;
	digidoc::X509Cert cert() const final;
	ErrorCode decrypt(const QByteArray &in, QByteArray &out, const QString &digest, int keySize,
		const QByteArray &algorithmID, const QByteArray &partyUInfo, const QByteArray &partyVInfo);
	QSslKey key() const;
	void logout();
	void selectCard(const TokenData &token);
	std::vector<unsigned char> sign( const std::string &method,
		const std::vector<unsigned char> &digest) const final;
	QSmartCard * smartcard() const;
	TokenData tokenauth() const;
	TokenData tokensign() const;

Q_SIGNALS:
	void cacheChanged();
	void authDataChanged( const TokenData &token );
	void signDataChanged( const TokenData &token );
	void error( const QString &msg );

private:
	static bool cardsOrder(const TokenData &s1, const TokenData &s2);
	void run() final;

	class Private;
	Private *d;
};
