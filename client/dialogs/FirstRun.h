// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QDialog>

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
	void keyPressEvent(QKeyEvent *event) final;
	void loadImages();

	enum View
	{
		Language,
		Intro,
		Signing,
		Encryption,
		MyEid
	};

	Ui::FirstRun *ui;
};

