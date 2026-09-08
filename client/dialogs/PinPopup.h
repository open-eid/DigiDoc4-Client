// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QDialog>

#include "QSmartCard.h"
#include "WaitDialog.h"

class LineEdit;
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
	~PinPopup() final;

	void setPinLen(unsigned long minLen, unsigned long maxLen = 12);
	QByteArray takePin();

signals:
	void startTimer();

private:
	void reject() final;

	LineEdit *pinInput;
	QRegularExpressionValidator *regexp {};
	WaitDialogHider hider;
	bool isPinPad = false;
};
