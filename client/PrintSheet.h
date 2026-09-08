// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QtCore/QCoreApplication>
#include <QtGui/QPainter>

class DigiDoc;
class QPrinter;

class PrintSheet: public QPainter
{
	Q_DECLARE_TR_FUNCTIONS( PrintSheet )

public:
	PrintSheet( DigiDoc *, QPrinter * );

private:
	void newPage( int height );
	void customText( const QString &title, const QString &text );
	int drawTextRect(QRect rect, const QString &text);

	int left, right, margin, top, bottom;
	QPrinter *p;
};
