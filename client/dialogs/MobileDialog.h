// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QDialog>

namespace Ui { class MobileDialog; }

class MobileDialog final : public QDialog
{
	Q_OBJECT

public:
	explicit MobileDialog(QWidget *parent = nullptr);
	~MobileDialog() final;

	QString idCode();
	QString phoneNo();

private:
	Ui::MobileDialog *ui;
};

