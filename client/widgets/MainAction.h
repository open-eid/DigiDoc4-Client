// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "common_enums.h"

#include <QWidget>

class MainAction final : public QWidget
{
	Q_OBJECT

public:
	explicit MainAction(QWidget *parent);
	~MainAction() final;

	void hideDropdown();
	void setButtonEnabled(bool enabled);
	void showActions(QList<ria::qdigidoc4::Actions> actions);

signals:
	void action(ria::qdigidoc4::Actions action);

private:
	void changeEvent(QEvent* event) override;
	bool eventFilter(QObject *watched, QEvent *event) override;
	void showDropdown();
	void update();

	static QString label(ria::qdigidoc4::Actions action);

	class Private;
	Private *ui;
};
