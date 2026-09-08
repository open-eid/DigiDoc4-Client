// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QtWidgets/QToolButton>

class QSvgWidget;

class PageIcon final : public QToolButton
{
	Q_OBJECT

public:
	explicit PageIcon( QWidget *parent = nullptr );

	void invalidIcon( bool show );
	void warningIcon( bool show );

private:
	void updateIcon();

	QSvgWidget *errorIcon;
	bool warn = false;
	bool err = false;
};
