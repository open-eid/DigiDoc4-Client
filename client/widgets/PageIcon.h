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
#include <QFont>
#include <QPaintEvent>
#include <QString>
#include <QSvgWidget>
#include <QWidget>

#include <memory>


namespace Ui {
class PageIcon;
}

class PageIcon : public QPushButton
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

protected:
	void enterEvent( QEvent *ev ) override;
	void leaveEvent( QEvent *ev ) override;
	void changeEvent(QEvent* event) override;

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

	void updateIcon();

	Ui::PageIcon *ui;
	QWidget *shadow = nullptr;
	std::unique_ptr<QSvgWidget> brightRedIcon;
	std::unique_ptr<QSvgWidget> filledOrangeIcon;
	std::unique_ptr<QSvgWidget> icon;
	std::unique_ptr<QSvgWidget> redIcon;
	std::unique_ptr<QSvgWidget> orangeIcon;

	Style active;
	IconType iconType = IconType::None;
	Style inactive;
	Style hover;
	bool selected = false;
	ria::qdigidoc4::Pages type = ria::qdigidoc4::Pages::SignIntro;

	void updateSelection();
	void updateSelection(const Style &style);
};
