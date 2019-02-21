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

#include <common/TokenData.h>

#include <QDialog>
#include <QtCore/QRegExp>

namespace Ui {
class PinPopup;
}

class SslCertificate;

class PinPopup : public QDialog
{
	Q_OBJECT

public:
	enum PinFlags
	{
		Pin1Type = 0,
		Pin2Type = 1 << 0,
		PukType = 1 << 1,
		PinpadFlag = 1 << 2,
		PinpadNoProgressFlag = 1 << 3,
		PinpadChangeFlag = 1 << 4,
		Pin1PinpadType = Pin1Type|PinpadFlag,
		Pin2PinpadType = Pin2Type|PinpadFlag,
		PukPinpadType = PukType|PinpadFlag
	};

	PinPopup(PinFlags flags, const TokenData &t, QWidget *parent = nullptr);
	PinPopup(PinFlags flags, const SslCertificate &cert, TokenData::TokenFlags token, QWidget *parent = nullptr);
	PinPopup(PinFlags flags, const QString &title, TokenData::TokenFlags token, QWidget *parent = nullptr, const QString &bodyText = QString());
	~PinPopup() final;

	void setPinLen(unsigned long minLen, unsigned long maxLen = 12);
	QString text() const;

signals:
	void startTimer();

private:
	Ui::PinPopup *ui;
	QRegExp		regexp;
};

