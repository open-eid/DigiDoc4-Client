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

#include <QCryptographicHash>

#include <functional>

class QCryptoBackend;
class QSmartCard;
class QSslKey;
class TokenData;

class QSigner final: public QThread, public digidoc::Signer
{
	Q_OBJECT

public:
	explicit QSigner(QObject *parent = nullptr);
	~QSigner() final;

	QList<TokenData> cache() const;
	digidoc::X509Cert cert() const final;
	QByteArray decrypt(std::function<QByteArray (QCryptoBackend *)> &&func);
	QSslKey key() const;
	void logout() const;
	void selectCard(const TokenData &token);
	std::vector<unsigned char> sign( const std::string &method,
		const std::vector<unsigned char> &digest) const final;
	QSmartCard * smartcard() const;
	TokenData tokenauth() const;
	TokenData tokensign() const;
	void setCachePIN(bool cache_pin);

Q_SIGNALS:
	void cacheChanged();
	void authDataChanged( const TokenData &token );
	void signDataChanged( const TokenData &token );
	void error( const QString &msg );

private:
	static bool cardsOrder(const TokenData &s1, const TokenData &s2);
	quint8 login(const TokenData &cert) const;
	static QCryptographicHash::Algorithm methodToNID(const std::string &method);
	void run() final;

	class Private;
	Private *d;
};
