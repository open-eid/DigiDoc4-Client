// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

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
