// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "DigiDoc.h"

#include <QDialog>

namespace Ui {
	class SignatureDialog;
}

class QDateTime;
class QLabel;
class QTreeWidget;
class SslCertificate;

class SignatureDialog : public QDialog
{
	Q_OBJECT

public:
	explicit SignatureDialog(const DigiDocSignature &signature, QWidget *parent = nullptr);
	~SignatureDialog() final;

private:
	static QPushButton *itemButton(const QString &text, QTreeWidget *view);
	static QLabel *itemLabel(const QString &text, QTreeWidget *view);
	void addItem(QTreeWidget *view, const QString &variable, QWidget *value);
	void addItem(QTreeWidget *view, const QString &variable, const QString &value);
	void addItem(QTreeWidget *view, const QString &variable, const QDateTime &value);
	void addItem(QTreeWidget *view, const QString &variable, SslCertificate cert);
	void addItem(QTreeWidget *view, const QString &variable, const QUrl &url);

	DigiDocSignature s;
	Ui::SignatureDialog *d;
};
