/*
 * QDigiDoc4
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

#include "common_enums.h"

#include <QFont>
#include <QPaintEvent>
#include <QString>
#include <QSvgWidget>
#include <QWidget>

#include <memory>


namespace Ui {
class PageIcon;
}

class PageIcon : public QWidget
{
	Q_OBJECT

public:
	explicit PageIcon( QWidget *parent = nullptr );
	~PageIcon();

	void init( ria::qdigidoc4::Pages page, QWidget *shadow, bool selected );
	void activate( bool selected );
	ria::qdigidoc4::Pages getType();
	
signals:
	void activated( PageIcon *const );

protected:
	void enterEvent( QEvent *ev ) override;
	void leaveEvent( QEvent *ev ) override;
	void mouseReleaseEvent( QMouseEvent *event ) override;
	void paintEvent( QPaintEvent *ev ) override;

private:
	struct Style
	{
		QFont font;
		QString image, backColor, foreColor;
	};

	Ui::PageIcon *ui;
	QWidget *shadow;
	std::unique_ptr<QSvgWidget> icon;
	Style active;
	Style inactive;
	Style hover;
	bool selected;
	ria::qdigidoc4::Pages type;

	void updateSelection();
};
