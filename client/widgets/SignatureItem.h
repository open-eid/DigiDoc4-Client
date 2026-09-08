// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "widgets/Item.h"

#include "widgets/WarningItem.h"

class DigiDocSignature;

class SignatureItem final : public Item
{
	Q_OBJECT

public:
	explicit SignatureItem(DigiDocSignature s, QWidget *parent = nullptr);
	~SignatureItem() final;

	WarningText::WarningType getError() const;
	void initTabOrder(QWidget *item) final;
	bool isSelfSigned(const QString& cardCode) const;
	QWidget* lastTabWidget() final;

private:
	bool event(QEvent *event) final;
	bool eventFilter(QObject *o, QEvent *e) final;
	void init();
	void elideRole();

	struct Private;
	Private *ui;
};
