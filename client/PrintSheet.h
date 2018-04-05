/*
 * QDigiDocClient
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
	int drawTextRect( const QRect &rect, const QString &text );

	int left, right, margin, top, bottom;
	QPrinter *p;
};
