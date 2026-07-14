// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once
#include <QtWidgets/QStackedWidget>

class ShrinkingStack final : public QStackedWidget
{
public:
	using QStackedWidget::QStackedWidget;
	QSize sizeHint() const final {
		return currentWidget() ? currentWidget()->sizeHint() : QStackedWidget::sizeHint();
	}
	QSize minimumSizeHint() const final {
		return currentWidget() ? currentWidget()->minimumSizeHint() : QStackedWidget::minimumSizeHint();
	}
};
