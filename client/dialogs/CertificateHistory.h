// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

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
