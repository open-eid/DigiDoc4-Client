// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "widgets/StyledWidget.h"

class QLabel;
class SslCertificate;

class Item : public StyledWidget
{
	Q_OBJECT

public:
	using StyledWidget::StyledWidget;

	virtual void idChanged(const SslCertificate &cert);
	virtual void initTabOrder(QWidget *item);
	virtual QWidget* lastTabWidget();

signals:
	void add(Item* item);
	void remove(Item* item);
};


class LabelItem : public Item
{
	Q_OBJECT

public:
	LabelItem(const char *text, QWidget *parent = nullptr);
	void initTabOrder(QWidget *item) override;

private:
	void changeEvent(QEvent *event) override;
	QLabel *label;
	const char *_text;
};
