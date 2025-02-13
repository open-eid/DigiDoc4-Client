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

#include "CryptoDoc.h"

#include <QDialog>

namespace Ui { class CertificateHistory; }
class SslCertificate;

class HistoryCertData
{
public:
	QString CN;
	QString type;
	QString issuer;
	QString expireDate;

	static QString toType(const SslCertificate &cert);

	bool operator==(const HistoryCertData& other) const noexcept = default;
	QString typeName() const;
};

class HistoryList: public QList<HistoryCertData>
{
public:
	HistoryList();

	void addAndSave(const SslCertificate &cert);
	void removeAndSave(const QList<HistoryCertData> &data);

private:
	void save();

	QString path;
};


class CertificateHistory: public QDialog
{
	Q_OBJECT

public:
	enum KeyType
	{
		IDCard = 0,
		TEMPEL = 1,
		DigiID = 2,
		Other = 3
	};

	CertificateHistory(HistoryList &historyCertData, QWidget *parent = nullptr);
	~CertificateHistory();

signals:
	void addSelectedCerts(const QList<HistoryCertData>& selectedCertData);

private:
	void fillView();
	QList<HistoryCertData> selectedItems() const;

	Ui::CertificateHistory *ui;
	HistoryList &historyCertData;
};
