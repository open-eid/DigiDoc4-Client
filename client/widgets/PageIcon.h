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

#include <QtWidgets/QPushButton>

class QSvgWidget;

namespace Ui {
class PageIcon;
}

class PageIcon final : public QPushButton
{
	Q_OBJECT

public:
	explicit PageIcon( QWidget *parent = nullptr );
	~PageIcon() final;

	void init( ria::qdigidoc4::Pages page, QWidget *shadow, bool selected );
	void activate( bool selected );
	ria::qdigidoc4::Pages getType();
	void invalidIcon( bool show );
	void warningIcon( bool show );

signals:
	void activated( PageIcon *icon );

private:
	enum IconType {
		None,
		Warning,
		Error
	};
	struct Style
	{
		QFont font;
		QString image, backColor, foreColor, border;
	};

	void changeEvent(QEvent *event) final;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	void enterEvent(QEnterEvent *ev) final;
#else
	void enterEvent(QEvent *ev) final;
#endif
	void leaveEvent(QEvent *event) final;
	void updateIcon();
	void updateSelection();
	void updateSelection(const Style &style);

	Ui::PageIcon *ui;
	QWidget *shadow = nullptr;
	QSvgWidget *errorIcon;
	QSvgWidget *icon;

	Style active;
	IconType iconType = IconType::None;
	Style inactive;
	Style hover;
	bool selected = false;
	ria::qdigidoc4::Pages type = ria::qdigidoc4::Pages::SignIntro;
};
