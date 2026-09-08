// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QDialog>

class QAbstractButton;

namespace Ui {
class FirstRun;
}

class FirstRun final: public QDialog
{
	Q_OBJECT

public:
	explicit FirstRun(QWidget *parent = nullptr);
	~FirstRun();

signals:
	void langChanged(const QString& lang);

private:
	void loadImages();

	enum View
	{
		Language,
		Intro,
		Signing,
		Encryption,
		MyEid
	};

	void setView(QAbstractButton *button);

	Ui::FirstRun *ui;
};
