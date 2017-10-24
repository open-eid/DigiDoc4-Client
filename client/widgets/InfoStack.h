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

#include "QSmartCard.h"

#include "Styles.h"
#include "widgets/StyledWidget.h"

namespace Ui {
class InfoStack;
}

class InfoStack : public StyledWidget, public PictureInterface
{
	Q_OBJECT

public:
	explicit InfoStack( QWidget *parent = nullptr );
	~InfoStack();

	void clearData();
	void clearPicture();
	void update( const QSmartCardData &t );
	void showPicture( const QPixmap &pixmap ) override;

signals:
	void photoClicked( const QPixmap *pixmap );

protected:
	void changeEvent(QEvent* event) override;

private:
	void focusEvent(int eventType);

	Ui::InfoStack *ui;
};
