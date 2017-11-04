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
#include "widgets/AccordionTitle.h"
#include "widgets/CardPopup.h"
#include "widgets/DropdownButton.h"
#include "widgets/NoCardInfo.h"
#include "widgets/PageIcon.h"

#include <QButtonGroup>
#include <QImage>
#include <QWidget>

class DigiDoc;

namespace Ui {
class MainWindow;
}

class CryptoDoc;
class DigiDoc;
class DocumentModel;

class MainWindow : public QWidget
{
	Q_OBJECT

public:
	explicit MainWindow( QWidget *parent = 0 );
	~MainWindow();

private Q_SLOTS:
	void pageSelected( PageIcon *const );
	void buttonClicked( int button );
	void showCardStatus();
	void photoClicked( const QPixmap *photo );
	void savePhoto( const QPixmap *photo );
	void getEmailStatus();
	void activateEmail ();
	void changePin1Clicked( bool isForgotPin, bool isBlockedPin );
	void changePin2Clicked( bool isForgotPin, bool isBlockedPin );
	void changePukClicked( bool isForgotPuk );
	void certDetailsClicked( const QString &link );
	void getOtherEID ();
	void operation(int op, bool started);
	void updateCertificate ( const QString &link );

protected:
	void changeEvent(QEvent* event) override;
	void dragEnterEvent( QDragEnterEvent *event ) override;
	void dragLeaveEvent( QDragLeaveEvent *event ) override;
	void dropEvent( QDropEvent *event ) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void resizeEvent( QResizeEvent *event ) override;

private:
	void cachePicture( const QString &id, const QImage &image );
	void clearOverlay();
	void convertToBDoc();
	void convertToCDoc();
	ria::qdigidoc4::ContainerState currentState();
	bool decrypt();
	bool encrypt();
	void hideCardPopup();
	void hideWarningArea();	
	void getMobileIdStatus ();
	void getDigiIdStatus ();
	void isUpdateCertificateNeeded();
	void loadPicture();
	void moveCryptoContainer();
	void moveSignatureContainer();
	void navigateToPage( ria::qdigidoc4::Pages page, const QStringList &files = QStringList(), bool create = true );
	void noReader_NoCard_Loading_Event(NoCardInfo::Status status);
	void onCryptoAction(int code, const QString &id, const QString &phone);
	void onSignAction(int code, const QString &idCode, const QString &phoneNumber);
	void openContainer();
	void openFiles( const QStringList files );
	void pinUnblock( QSmartCardData::PinType type, bool isForgotPin = false );
	void pinPukChange( QSmartCardData::PinType type );
	void removeAddress(int index);
	void removeCryptoFile(int index);
	bool removeFile(DocumentModel *model, int index);
	void removeSignature(int index);
	void removeSignatureFile(int index);
	bool save();
	QString selectFile( const QString &filename, bool fixedExt );
	void selectPageIcon( PageIcon* page );
	QByteArray sendRequest( SSLConnect::RequestType type, const QString &param = QString() );
	void showCardMenu( bool show );
	void showOverlay( QWidget *parent );
	void showNotification( const QString &msg, bool isSuccess = false );
	void showWarning( const QString &msg, const QString &details, bool extLink = false );
	bool sign();
	bool signMobile(const QString &idCode, const QString &phoneNumber);
	void updateCardData();
	bool validateCardError( QSmartCardData::PinType type, int flags, QSmartCard::ErrorType err );
	void containerToEmail( const QString &fileName );
	void browseOnDisk( const QString &fileName );
	void showUpdateCertWarning();
	void showIdCardAlerts(const QSmartCardData& t);
	bool wrapContainer();
	
	CryptoDoc* cryptoDoc = nullptr;
	DigiDoc* digiDoc = nullptr;

	Ui::MainWindow *ui;

	std::unique_ptr<CardPopup> cardPopup;
	std::unique_ptr<Overlay> overlay;
	DropdownButton *selector = nullptr;
	QSmartCard *smartcard = nullptr;
	QButtonGroup *buttonGroup = nullptr;
};
