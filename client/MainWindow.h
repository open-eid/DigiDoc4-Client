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
#include "QSmartCard.h"
#include "sslConnect.h"
#include "effects/Overlay.h"
#include "widgets/NoCardInfo.h"

#include <QButtonGroup>

#include <memory>

namespace Ui {
class MainWindow;
}

class CKey;
class CryptoDoc;
class DigiDoc;
class DropdownButton;
class DocumentModel;
class PageIcon;
class WarningList;

class MainWindow : public QWidget
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow() final;

	QString cryptoPath();
	QString digiDocPath();
	void showSettings(int page);

signals:
	void clearPopups();

private Q_SLOTS:
	void activateEmail ();
	void buttonClicked( int button );
	void changePin1Clicked( bool isForgotPin, bool isBlockedPin );
	void changePin2Clicked( bool isForgotPin, bool isBlockedPin );
	void changePukClicked();
	void getEmailStatus();
	void open(const QStringList &params, bool crypto, bool sign);
	void openFile(const QString &file);
	void pageSelected(PageIcon *page);
	void photoClicked(const QPixmap &photo);
	void savePhoto();
	void showCardStatus();
	void warningClicked(const QString &link);

protected:
	void changeEvent(QEvent* event) final;
	void closeEvent(QCloseEvent *event) final;
	void dragEnterEvent( QDragEnterEvent *event ) final;
	void dragLeaveEvent( QDragLeaveEvent *event ) final;
	void dropEvent( QDropEvent *event ) final;
	void mouseReleaseEvent(QMouseEvent *event) final;
	void resizeEvent( QResizeEvent *event ) final;
	void showEvent(QShowEvent *event) final;

private:
	void adjustDrops();
	void browseOnDisk(const QString &fileName);
	void clearOverlay();
	void containerToEmail(const QString &fileName);
	void convertToBDoc();
	void convertToCDoc();
	ria::qdigidoc4::ContainerState currentState();
	bool decrypt();
	QStringList dropEventFiles(QDropEvent *event) const;
	bool encrypt();
	void hideCardPopup();
	void loadPicture();
	void moveCryptoContainer();
	void moveSignatureContainer();
	void navigateToPage( ria::qdigidoc4::Pages page, const QStringList &files = QStringList(), bool create = true );
	void noReader_NoCard_Loading_Event(NoCardInfo::Status status);
	void onCryptoAction(int code, const QString &id, const QString &phone);
	void onSignAction(int code, const QString &info1, const QString &info2);
	void openContainer();
	void openFiles(const QStringList &files, bool addFile = false, bool forceCreate = false);
	void pinUnblock(QSmartCardData::PinType type, bool isForgotPin);
	void pinPukChange( QSmartCardData::PinType type );
	void resetCryptoDoc(CryptoDoc *doc = nullptr);
	void resetDigiDoc(DigiDoc *doc = nullptr, bool warnOnChange = true);
	void removeAddress(int index);
	void removeCryptoFile(int index);
	bool removeFile(DocumentModel *model, int index);
	void removeSignature(int index);
	void removeSignatureFile(int index);
	bool save(bool saveAs = false);
	QString selectFile( const QString &title, const QString &filename, bool fixedExt );
	void selectPage(ria::qdigidoc4::Pages page);
	void selectPageIcon( PageIcon* page );
	QByteArray sendRequest( SSLConnect::RequestType type, const QString &param = QString() );
	void showCardMenu( bool show );
	void showOverlay( QWidget *parent );
	void showNotification( const QString &msg, bool isSuccess = false );
	void sign(const std::function<bool(const QString &city, const QString &state, const QString &zip, const QString &country, const QString &role)> &sign);
	void updateCardWarnings(const QSmartCardData &data);
	bool validateCardError(QSmartCardData::PinType type, QSmartCardData::PinType t, QSmartCard::ErrorType err);
	bool validateFiles(const QString &container, const QStringList &files);
	void showPinBlockedWarning(const QSmartCardData& t);
	void updateKeys(const QList<CKey> &keys);
	void updateMyEid(const QSmartCardData &data);
	bool wrap(const QString& wrappedFile, bool enclose);
	bool wrapContainer(bool signing);
	void containerSummary();
	
	CryptoDoc* cryptoDoc = nullptr;
	DigiDoc* digiDoc = nullptr;

	Ui::MainWindow *ui;

	QButtonGroup *buttonGroup = nullptr;
	std::unique_ptr<Overlay> overlay;
	DropdownButton *selector = nullptr;
	WarningList *warnings;
};
