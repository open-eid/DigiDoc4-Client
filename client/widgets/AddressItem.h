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

#include "crypto/CryptoDoc.h"
#include "SslCertificate.h"

namespace Ui {
class AddressItem;
}

class AddressItem final : public Item
{
	Q_OBJECT

public:
	enum ShowToolButton
	{
		None,
		Remove,
		Add,
		Added
	};

	explicit AddressItem(CKey k, QWidget *parent = nullptr, bool showIcon = false);
	~AddressItem() final;

	void disable(bool disable);
	const CKey& getKey() const;
	void idChanged(const SslCertificate &cert) final;
	QWidget* initTabOrder(QWidget *item) final;
	void showButton(ShowToolButton show);
	void stateChange(ria::qdigidoc4::ContainerState state) final;

private:
	void changeEvent(QEvent *event) final;
	bool eventFilter(QObject *o, QEvent *e) final;
	void mouseReleaseEvent(QMouseEvent *event) final;
	void setName();
	void setIdType();

	Ui::AddressItem *ui;

	QString code;
	CKey key;
	QString name;
	QString expireDateText;
	bool yourself = false;
};
