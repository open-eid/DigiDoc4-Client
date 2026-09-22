// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QWidget>

#include <QDateTime>

namespace Ui {
class InfoStack;
}

class SslCertificate;
class QSmartCardData;

class InfoStack final: public QWidget
{
	Q_OBJECT

public:
	explicit InfoStack( QWidget *parent = nullptr );
	~InfoStack() final;

	void clearData();
	void update(const SslCertificate &cert);
	void update(const QSmartCardData &t);

private:
	void changeEvent(QEvent* event) final;
	void update();

	Ui::InfoStack *ui;

	QDateTime expiry;
	int certType = 0;
};
