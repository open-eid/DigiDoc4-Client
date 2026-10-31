// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QtCore/QCoreApplication>
#include <QtWidgets/QScrollArea>

class CardList final : public QScrollArea
{
	Q_DECLARE_TR_FUNCTIONS(CardList)
public:
	using QScrollArea::QScrollArea;
	QSize sizeHint() const final;
	QSize minimumSizeHint() const final;
	static QString emptyText();

protected:
	bool eventFilter(QObject *watched, QEvent *event) final;

private:
	static constexpr int VisibleCards = 2;
};
