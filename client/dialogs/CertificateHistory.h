/*
 * QDigiDocCrypto
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

#include "crypto/CryptoDoc.h"

#include <QDialog>

namespace Ui { class CertificateHistory; }

class HistoryCertData
{
public:
	QString CN;
	QString type;
	QString issuer;
	QString expireDate;

	bool operator==(const HistoryCertData& other);
};


class CertificateHistory: public QDialog
{
	Q_OBJECT

public:
	CertificateHistory(QList<HistoryCertData>& historyCertData, QWidget *parent = 0);
	~CertificateHistory();

	int exec() override;

signals:
	void addSelectedCetrs(const QList<HistoryCertData>& selectedCertData);
	void removeSelectedCetrs(const QList<HistoryCertData>& removeCertData);

protected:
	void fillView();
	void getSelectedItems(QList<HistoryCertData>& selectedCertData);

	void select();
	void remove();

	Ui::CertificateHistory *ui;
	QList<HistoryCertData>& historyCertData;
};
