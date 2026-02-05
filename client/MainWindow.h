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

#include <QWidget>

#include "common_enums.h"
#include "QSmartCard.h"

namespace Ui {
class MainWindow;
}

class CryptoDoc;
class DigiDoc;
class TokenData;

class MainWindow final : public QWidget
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow() final;

	void openFiles(QStringList files, bool addFile = false, bool forceCreate = false);
	void selectPage(ria::qdigidoc4::Pages page);
	void showSettings(int page);

protected:
	void changeEvent(QEvent* event) final;
	void closeEvent(QCloseEvent *event) final;
	void dragEnterEvent( QDragEnterEvent *event ) final;
	void dragLeaveEvent( QDragLeaveEvent *event ) final;
	void dropEvent( QDropEvent *event ) final;
	void mouseReleaseEvent(QMouseEvent *event) final;
	void showEvent(QShowEvent *event) final;

private:
	void adjustDrops();
	void changePinClicked(QSmartCardData::PinType type, QSmartCard::PinAction action);
	void convertToCDoc();
	ria::qdigidoc4::ContainerState currentState();
	bool encrypt(bool askForKey = false);
	void loadPicture();
	void navigateToPage( ria::qdigidoc4::Pages page, const QStringList &files = QStringList(), bool create = true );
	void onCryptoAction(int action, const QString &id, const QString &phone);
	void onSignAction(int action, const QString &idCode, const QString &info2);
	void openContainer(bool signature);
	void resetDigiDoc(std::unique_ptr<DigiDoc> &&doc);
	void removeSignature(int index);
	template <typename F>
	void sign(F &&sign);
	void updateSelector();
	void updateMyEID(const TokenData &t);
	void updateMyEid(const QSmartCardData &data);
	bool wrap(const QString& wrappedFile, bool pdf);
	bool wrapContainer(bool signing);

	static QStringList dropEventFiles(QDropEvent *event);

	std::unique_ptr<CryptoDoc> cryptoDoc;
	std::unique_ptr<DigiDoc> digiDoc;
	Ui::MainWindow *ui;
};
