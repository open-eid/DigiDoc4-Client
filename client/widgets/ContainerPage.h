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
#include "widgets/ItemList.h"
#include "widgets/MainAction.h"

#include <QPushButton>
#include <QResizeEvent>
#include <QWidget>
#include <vector>
#include <memory>


namespace Ui {
class ContainerPage;
}

class ContainerPage : public QWidget
{
	Q_OBJECT

public:
	explicit ContainerPage( QWidget *parent = nullptr );
	~ContainerPage();

	void transition( ria::qdigidoc4::ContainerState state, const QStringList &files = QStringList() );
	void showWarningText( const QString &text, const QString &link );
	void hideWarningArea();
	void setContainer( const QString &file );

signals:
	void action( int code );

protected:
	void mousePressEvent( QMouseEvent *event ) override;
	void resizeEvent( QResizeEvent *event ) override;

private:
	void init();
	void initContainer( const QString &file, const QString &suffix );
	void hideButtons( std::vector<QWidget*> buttons );
	void hideMainAction();
	void hideRightPane();
	void showButtons( std::vector<QWidget*> buttons );
	void showRightPane( ItemList::ItemType itemType, const QString &header );
	void showMainAction( ria::qdigidoc4::Actions action, const QString &label );

	Ui::ContainerPage *ui;
	std::unique_ptr<MainAction> mainAction;
};
