// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QDialog>

namespace Ui {
class PasswordDialog;
}

class PasswordDialog final : public QDialog
{
	Q_OBJECT

public:
	enum Mode : quint8 {
		ENCRYPT,
		DECRYPT
	};

	explicit PasswordDialog(Mode mode, QWidget *parent = nullptr);
	~PasswordDialog() final;

	void setLabel(const QString& label);
	QString label();
	QByteArray secret() const;

private:
	Ui::PasswordDialog *ui;
};
