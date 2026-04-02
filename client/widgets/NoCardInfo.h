// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QWidget>

namespace Ui {
class NoCardInfo;
}

class NoCardInfo : public QWidget
{
	Q_OBJECT

public:
	enum Status {
		NoCard,
		NoPCSC,
		NoReader,
		Loading
	};

	explicit NoCardInfo( QWidget *parent = nullptr );
	~NoCardInfo() final;

	void update(Status s);

protected:
	void changeEvent(QEvent* event) final;

private:
	Ui::NoCardInfo *ui;
	Status status = Loading;
};
