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
#include "widgets/AddressItem.h"
#include "widgets/ItemList.h"

#include <QDialog>
#include <QMap>


namespace Ui {
class AddRecipients;
}

class LdapSearch;
class QSslCertificate;

class AddRecipients : public QDialog
{
	Q_OBJECT

public:
	explicit AddRecipients(ItemList* itemList, QWidget *parent = 0);
	~AddRecipients();

	int exec() override;
	QList<CKey> keys();
	bool isUpdated();

private:
	void init();

	void addAllRecipientToRightPane();
	void addRecipientFromCard();
	void addRecipientFromFile();
	void addRecipientFromHistory();
	void addRecipientToLeftPane(const QSslCertificate& cert);
	void addRecipientToRightPane(Item* toAdd, bool update = true);

	void addSelectedCerts(const QList<HistoryCertData>& selectedCertData);

	void enableRecipientFromCard();
	void initAddressItems(const std::vector<Item*>& items);

	QString path() const;
	void removeRecipientFromRightPane(Item *toRemove);
	void removeSelectedCerts(const QList<HistoryCertData>& removeCertData);

	void search(const QString &term);
	void showError(const QString &msg, const QString &details = QString());
	void showResult(const QList<QSslCertificate> &result);

	Ui::AddRecipients *ui;
	QMap<QString, AddressItem *> leftList;
	QStringList rightList;
	LdapSearch* ldap;
	bool updated;

	QList<HistoryCertData> historyCertData;
};
