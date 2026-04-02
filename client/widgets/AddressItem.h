// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "widgets/Item.h"

class CKey;

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

	explicit AddressItem(CKey &&k, Type type, QWidget *parent = {});
	~AddressItem() final;

	const CKey& getKey() const;
	void idChanged(const SslCertificate &cert) final;
	void initTabOrder(QWidget *item) final;
	QWidget* lastTabWidget() final;
	void stateChange(ria::qdigidoc4::ContainerState state) final;

private:
	void changeEvent(QEvent *event) final;
	void mouseReleaseEvent(QMouseEvent *event) final;
	void setName();
	void setIdType();

	class Private;
	Private *ui;
};
