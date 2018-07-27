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

#include "QSmartCard.h"

#include <QtCore/QThread>
#include <digidocpp/crypto/Signer.h>

struct QCardInfo;
class TokenData;

class QSigner: public QThread, public digidoc::Signer
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
	~QSigner();

	const QMap<QString, QSharedPointer<QCardInfo>> cache() const;
	digidoc::X509Cert cert() const override;
	ErrorCode decrypt(const QByteArray &in, QByteArray &out, const QString &digest, int keySize,
		const QByteArray &algorithmID, const QByteArray &partyUInfo, const QByteArray &partyVInfo);
	QSslKey key() const;
	void logout();
	std::vector<unsigned char> sign( const std::string &method,
		const std::vector<unsigned char> &digest ) const override;
	QSmartCard * smartcard() const;
	TokenData tokenauth() const;
	TokenData tokensign() const;

Q_SIGNALS:
	void authDataChanged( const TokenData &token );
	void dataChanged();
	void signDataChanged( const TokenData &token );
	void error( const QString &msg );

private Q_SLOTS:
	void cacheCardData(const QSet<QString> &cards);
	void selectCard(const QString &card);

private:
	void reloadauth() const;
	void reloadsign() const;
	void run() override;

	class Private;
	Private *d;

	friend class MainWindow;
};
