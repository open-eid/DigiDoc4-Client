// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QDialog>

#include "QSmartCard.h"
#include "WaitDialog.h"

class PinpadPopup final : public QDialog
{
	Q_OBJECT

public:
	PinpadPopup(QSmartCardData::PinType type, QSmartCard::PinAction action,
		const QSmartCardData &current, QWidget *parent = {});

private:
	void reject() final;

	WaitDialogHider hider;
};

