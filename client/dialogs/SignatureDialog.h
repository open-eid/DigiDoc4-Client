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

#include "DigiDoc.h"

#include <QDialog>

namespace Ui {
	class SignatureDialog;
}

class QDateTime;
class QLabel;
class QTreeWidget;

class SignatureDialog : public QDialog
{
	Q_OBJECT

public:
	explicit SignatureDialog(const DigiDocSignature &signature, QWidget *parent = nullptr);
	~SignatureDialog() final;

private:
	QPushButton *itemButton(const QString &text, QTreeWidget *view);
	QLabel *itemLabel(const QString &text, QTreeWidget *view);
	void addItem(QTreeWidget *view, const QString &variable, QWidget *value);
	void addItem(QTreeWidget *view, const QString &variable, const QString &value);
	void addItem(QTreeWidget *view, const QString &variable, const QDateTime &value);
	void addItem(QTreeWidget *view, const QString &variable, const QSslCertificate &cert);
	void addItem(QTreeWidget *view, const QString &variable, const QUrl &url);
	void decorateNotice(const QString &color);

	DigiDocSignature s;
	Ui::SignatureDialog *d;
};
