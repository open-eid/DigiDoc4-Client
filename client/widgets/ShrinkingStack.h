// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once
#include <QtWidgets/QLayout>
#include <QtWidgets/QStackedWidget>

class ShrinkingStack final : public QStackedWidget
{
public:
	explicit ShrinkingStack(QWidget *parent = nullptr)
		: QStackedWidget(parent)
	{
		// QStackedLayout would set the minimum size to that of the largest page;
		// the pages not shown must not hold the size
		layout()->setSizeConstraint(QLayout::SetNoConstraint);
		connect(this, &QStackedWidget::currentChanged, this, &QWidget::updateGeometry);
	}
	QSize sizeHint() const final {
		return currentWidget() ? currentWidget()->sizeHint() : QStackedWidget::sizeHint();
	}
	QSize minimumSizeHint() const final {
		return currentWidget() ? currentWidget()->minimumSizeHint() : QStackedWidget::minimumSizeHint();
	}
	// A page with wrapped labels would make the parent layout ask for height-for-width,
	// which QWidgetItem takes from QStackedLayout: the tallest of all pages, hidden ones
	// included. Without it the layout falls back to sizeHint(), the current page's.
	bool hasHeightForWidth() const final {
		return false;
	}
};
