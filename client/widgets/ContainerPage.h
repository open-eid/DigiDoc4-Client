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
#include "crypto/CryptoDoc.h"
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

class DigiDoc;
class QFont;
class QFontMetrics;

class ContainerPage : public QWidget
{
	Q_OBJECT

public:
	explicit ContainerPage( QWidget *parent = nullptr );
	~ContainerPage();

	void setHeader(const QString &file);
	void transition(CryptoDoc *container);
	void transition(DigiDoc* container);

signals:
	void action(int code, const QString &idCode = QString(), const QString &phoneNumber = QString());
	void cardChanged(const QString& idCode = QString());
	void removed(int row);

protected:
	void resizeEvent( QResizeEvent *event ) override;

private:
	void changeCard(const QString& idCode);
	void clear();
	void elideFileName(bool force = false);
	void forward(int code);
	void browseContainer(QString link);
	void hideButtons( std::vector<QWidget*> buttons );
	void hideMainAction();
	void hideOtherAction();
	void hideRightPane();
	void init();
	void initContainer( const QString &file, const QString &suffix );
	void mobileDialog();
	void showButtons( std::vector<QWidget*> buttons );
	void showDropdown();
	void showMainAction( ria::qdigidoc4::Actions action, const QString &label );
	void showRightPane(ria::qdigidoc4::ItemType itemType, const QString &header);
	void showSigningButton();
	void updatePanes(ria::qdigidoc4::ContainerState state);

	Ui::ContainerPage *ui;
	std::unique_ptr<MainAction> mainAction;
	std::unique_ptr<MainAction> otherAction;
	std::vector<QMetaObject::Connection> actionConnections;
	QString cardInReader;
	int containerFileWidth;
	QFont containerFont;
	bool elided;
	QString fileName;
	QFontMetrics fm;
	QString mobileCode;
};
