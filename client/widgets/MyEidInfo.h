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

#include <QWidget>

#include <QDateTime>

namespace Ui {
class MyEidInfo;
}

class SslCertificate;
class QSmartCardData;

class MyEidInfo final: public QWidget
{
	Q_OBJECT

public:
	explicit MyEidInfo( QWidget *parent = nullptr );
	~MyEidInfo() final;

	void clearData();
	void update(const SslCertificate &cert);
	void update(const QSmartCardData &t);

Q_SIGNALS:
	void changePinClicked(QSmartCardData::PinType, QSmartCard::PinAction);

private:
	void changeEvent(QEvent* event) final;
	void update();

	Ui::MyEidInfo *ui;

	QDateTime expiry;
	int certType = 0;
};
