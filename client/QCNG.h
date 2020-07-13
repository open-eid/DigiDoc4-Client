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

#include "QWin.h"

class QCNG: public QWin
{
	Q_OBJECT
public:
	explicit QCNG(QObject *parent = nullptr);
	~QCNG() override;

	QList<TokenData> tokens() const override;
	QByteArray decrypt(const QByteArray &data) const override;
	QByteArray deriveConcatKDF(const QByteArray &publicKey, const QString &digest, int keySize,
		const QByteArray &algorithmID, const QByteArray &partyUInfo, const QByteArray &partyVInfo) const override;
	PinStatus lastError() const override;
	PinStatus login(const TokenData &token) override;
	QByteArray sign(int method, const QByteArray &digest) const override;

private:
	class Private;
	Private *d;
};
