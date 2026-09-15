// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QDialog>

namespace Ui { class SmartIDDialog; }

class SmartIDDialog final : public QDialog
{
	Q_OBJECT

public:
	explicit SmartIDDialog(QWidget *parent = nullptr);
	~SmartIDDialog() final;

	QString country() const;
	QString idCode() const;

private:
	Ui::SmartIDDialog *ui;
};
