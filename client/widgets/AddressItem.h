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

#include "widgets/Item.h"

#include <vector>

struct CKey;

class AddressItem final : public Item
{
	Q_OBJECT

public:
	enum Type
	{
		Add,
		Remove,
		Icon,
	};

	explicit AddressItem(const CKey &k, Type type, QWidget *parent = {});
	~AddressItem() final;

	// Adds another key of the same person to this card
	void addKey(const CKey &key);
	const CKey& getKey() const;
	const std::vector<CKey>& keys() const;
	// Keys ticked on an Add card, all keys on the others
	std::vector<CKey> selectedKeys() const;
	// Recipient as shown on the row, as plain text
	QString name() const;
	void initTabOrder(QWidget *item) final;
	QWidget* lastTabWidget() final;
	void stateChange(ria::qdigidoc4::ContainerState state) final;

signals:
	void decrypt();

private:
	void changeEvent(QEvent *event) final;
	bool eventFilter(QObject *watched, QEvent *event) final;
	void setAddEnabled();
	void mouseReleaseEvent(QMouseEvent *event) final;
	void setDecryptVisible();
	void setName();
	bool setRecipient(const CKey &key);
	void setKeyText(size_t i);
	QString typeName(const SslCertificate &cert) const;

	struct Private;
	Private *ui;
};
