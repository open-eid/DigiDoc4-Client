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

#include <QDialog>

#include "QSmartCard.h"
#include "WaitDialog.h"

class QLineEdit;
class QRegularExpressionValidator;
class SslCertificate;

class PinPopup final : public QDialog
{
	Q_OBJECT

public:
	enum TokenFlag
	{
		PinCountLow = 1<<1,
		PinFinalTry = 1<<2,
		PinLocked = 1<<3,
		PinpadFlag = 1 << 4,
		PinpadChangeFlag = 1 << 5,
	};
	Q_DECLARE_FLAGS(TokenFlags, TokenFlag)

	PinPopup(QSmartCardData::PinType type, TokenFlags flags, const SslCertificate &cert, QWidget *parent = {}, QString text = {});

	void setPinLen(unsigned long minLen, unsigned long maxLen = 12);
	QString pin() const;

signals:
	void startTimer();

private:
	void reject() final;

	QLineEdit *pinInput;
	QRegularExpressionValidator *regexp {};
	WaitDialogHider hider;
	bool isPinPad = false;
};

