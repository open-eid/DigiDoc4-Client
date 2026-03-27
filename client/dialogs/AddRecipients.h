// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "CertificateHistory.h"

#include <QDialog>

namespace Ui {
class AddRecipients;
}

class AddressItem;
class Item;
class ItemList;
class LdapSearch;
class QSslCertificate;

class AddRecipients final : public QDialog
{
	Q_OBJECT

public:
	explicit AddRecipients(ItemList* itemList, QWidget *parent = nullptr);
	~AddRecipients() final;

	QList<CKey> keys() const;
	bool isUpdated() const;

private:
	void addRecipientFromFile();
	void addRecipientFromHistory();
	void addRecipient(const QSslCertificate& cert, bool select = true);
	void addRecipientToRightPane(Item *item, bool update = true);

	void search(const QString &term, bool select = false, const QString &type = {});
	void showError(const QString &title, const QString &details);
	void showResult(const QList<QSslCertificate> &result, int resultCount, const QVariantMap &userData);

	static AddressItem* itemListValue(ItemList *list, const CKey &cert);

	Ui::AddRecipients *ui;
	QList<CKey> rightList;
	QList<LdapSearch*> ldap_person;
	LdapSearch *ldap_corp;
	int multiSearch = 0;

	HistoryList historyCertData;
};
