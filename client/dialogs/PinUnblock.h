// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "QSmartCard.h"

#include <QDate>
#include <QDialog>

namespace Ui { class PinUnblock; }

class PinUnblock final : public QDialog
{
	Q_OBJECT

public:
	PinUnblock(QSmartCardData::PinType type, QSmartCard::PinAction action,
		 short leftAttempts, QDate birthDate, const QString &personalCode, bool isPUKReplacable, QWidget *parent);
	~PinUnblock() final;

	QString firstCodeText() const;
	QString newCodeText() const;

private:
	Ui::PinUnblock *ui;
};
