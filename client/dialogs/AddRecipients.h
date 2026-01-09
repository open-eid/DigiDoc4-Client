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
