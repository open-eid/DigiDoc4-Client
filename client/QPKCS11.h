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

#include "QCryptoBackend.h"

class QPKCS11: public QCryptoBackend
{
	Q_OBJECT
public:
	explicit QPKCS11(QObject *parent = nullptr);
	~QPKCS11() final;

	QByteArray decrypt(const QByteArray &data) const override;
	QByteArray derive(const QByteArray &publicKey) const;
	QByteArray deriveConcatKDF(const QByteArray &publicKey, const QString &digest, int keySize,
		const QByteArray &algorithmID, const QByteArray &partyUInfo, const QByteArray &partyVInfo) const override;
	bool isLoaded() const;
	PinStatus lastError() const override;
	bool load( const QString &driver );
	void unload();
	PinStatus login(const TokenData &t) override;
	void logout() override;
	bool reload();
	QByteArray sign(int type, const QByteArray &digest) const override;
	QList<TokenData> tokens() const override;
private:
	class Private;
	Private *d;
};
