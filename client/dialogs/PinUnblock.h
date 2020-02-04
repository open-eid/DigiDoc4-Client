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

#include <QDate>
#include <QDialog>

namespace Ui {
class PinUnblock;
}

class PinUnblock : public QDialog
{
	Q_OBJECT

public:
	enum WorkMode
	{
		UnBlockPinWithPuk = 1,
		ChangePinWithPuk = 2,
		PinChange = 4,
		PukChange = 8
	};
	PinUnblock(WorkMode mode, QWidget *parent, QSmartCardData::PinType type, short leftAttempts,
		QDate birthDate, QString personalCode);
	~PinUnblock() final;

	QString firstCodeText() const;
	QString newCodeText() const;

private:
	void init(  WorkMode mode, QSmartCardData::PinType type, short leftAttempts );
	void initIntro(WorkMode mode, QSmartCardData::PinType type);
	void setUnblockEnabled();
	bool validatePin(const QString& pin, QSmartCardData::PinType type, WorkMode mode);

	Ui::PinUnblock *ui;
	QDate birthDate;
	QString personalCode;
	QRegExp regexpFirstCode;
	QRegExp regexpNewCode;
	bool isFirstCodeOk = false;
	bool isNewCodeOk = false;
	bool isRepeatCodeOk = false;
};
