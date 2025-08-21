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

class CKey;
class CryptoDoc;
class DigiDoc;
class DocumentModel;
class PageIcon;
class TokenData;
class WarningList;

class MainWindow final : public QWidget
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow() final;

	void open(const QStringList &files, bool crypto, bool sign);
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
	void changePin1Clicked( bool isForgotPin, bool isBlockedPin );
	void changePin2Clicked( bool isForgotPin, bool isBlockedPin );
	void changePukClicked();
	void convertToBDoc();
	void convertToCDoc();
	ria::qdigidoc4::ContainerState currentState();
	bool decrypt();
	bool encrypt();
	void loadPicture();
	void moveCryptoContainer();
	void moveSignatureContainer();
	void navigateToPage( ria::qdigidoc4::Pages page, const QStringList &files = QStringList(), bool create = true );
	void onCryptoAction(int action, const QString &id, const QString &phone);
	void onSignAction(int action, const QString &info1, const QString &info2);
	void openContainer(bool signature);
	void openFiles(const QStringList &files, bool addFile = false, bool forceCreate = false);
	void pageSelected(int page, bool checked = true);
	void pinUnblock(QSmartCardData::PinType type, bool isForgotPin);
	void pinPukChange( QSmartCardData::PinType type );
	void resetCryptoDoc(std::unique_ptr<CryptoDoc> &&doc = {});
	void resetDigiDoc(DigiDoc *doc = nullptr, bool warnOnChange = true);
	void removeCryptoFile(int index);
	bool removeFile(DocumentModel *model, int index);
	void removeSignature(int index);
	void removeSignatureFile(int index);
	bool save(bool saveAs = false);
	QString selectFile( const QString &title, const QString &filename, bool fixedExt );
	void selectPage(ria::qdigidoc4::Pages page);
	template <typename F>
	void sign(F &&sign);
	void updateCardWarnings(const QSmartCardData &data);
	bool validateCardError(QSmartCardData::PinType type, QSmartCardData::PinType src, QSmartCard::ErrorType err);
	bool validateFiles(const QString &container, const QStringList &files);
	void showPinBlockedWarning(const QSmartCardData& t);
	void updateSelector();
	void updateSelectorData(TokenData data);
	void updateMyEID(const TokenData &t);
	void updateMyEid(const QSmartCardData &data);
	void warningClicked(const QString &link);
	bool wrap(const QString& wrappedFile, bool enclose);
	bool wrapContainer(bool signing);
	void containerSummary();

	static void browseOnDisk(const QString &fileName);
	static void containerToEmail(const QString &fileName);
	static QStringList dropEventFiles(QDropEvent *event);

	std::unique_ptr<CryptoDoc> cryptoDoc;
	DigiDoc* digiDoc = nullptr;
	Ui::MainWindow *ui;
};
