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

#include <memory>

#include "CryptoDoc.h"
#include "widgets/Item.h"

class AddressItem final : public Item
{
	Q_OBJECT

public:
	enum ShowToolButton
	{
		Remove,
		Add,
		Added,
	};

	explicit AddressItem(const CDKey &k, QWidget *parent = {}, bool showIcon = false);
	~AddressItem() final;

	const CDKey& getKey() const;
	void idChanged(const SslCertificate &cert) final;
	void initTabOrder(QWidget *item) final;
	QWidget* lastTabWidget() final;
	void showButton(ShowToolButton show);
	void stateChange(ria::qdigidoc4::ContainerState state) final;

signals:
	void decrypt(const libcdoc::Lock *lock);

private:
	void changeEvent(QEvent *event) final;
	void mouseReleaseEvent(QMouseEvent *event) final;
	void setName();
	void setIdType();
	void setIdType(const SslCertificate& cert);

	class Private;
	Private *ui;
};
