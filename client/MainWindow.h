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
#include "widgets/PageIcon.h"

#include <QButtonGroup>
#include <QImage>
#include <QMessageBox>
#include <QMimeData>
#include <QWidget>

namespace Ui {
class MainWindow;
}

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
	void loadCardPhoto();
	void getEmailStatus();
	void activateEmail ();
	void changePin1Clicked( bool isForgotPin, bool isBlockedPin );
	void changePin2Clicked( bool isForgotPin, bool isBlockedPin );
	void changePukClicked( bool isForgotPuk );
	void certDetailsClicked( const QString &link );

protected:
	void dragEnterEvent( QDragEnterEvent *event ) override;
	void dragLeaveEvent( QDragLeaveEvent *event ) override;
	void dropEvent( QDropEvent *event ) override;

private:
	enum ButtonTypes
	{
		PageEmpty = 0x00,

		PageCert = 0x01,
		PageCertAuthView = 0x11,
		PageCertSignView = 0x21,
		PageCertPin1 = 0x31,
		PageCertPin2 = 0x41,
		PageCertUpdate = 0x51,

		PageEmail = 0x02,
		PageEmailStatus = 0x12,
		PageEmailActivate = 0x22,

		PageMobile = 0x03,
		PageMobileStatus = 0x13,
		PageMobileActivate = 0x23,

		PagePukInfo = 0x04,

		PagePin1Pin = 0x05,
		PagePin1Puk = 0x15,
		PagePin1Unblock = 0x25,
		PagePin1ChangePin = 0x35,
		PagePin1ChangePuk = 0x45,
		PagePin1ChangeUnblock = 0x55,

		PagePin2Pin = 0x06,
		PagePin2Puk = 0x16,
		PagePin2Unblock = 0x26,
		PagePin2ChangePin = 0x36,
		PagePin2ChangePuk = 0x46,
		PagePin2ChangeUnblock = 0x56,

		PagePuk = 0x07,
		PagePukChange = 0x17
	};

	void noReader_NoCard_Loading_Event( const QString &text, bool isLoading = false );
	void cachePicture( const QString &id, const QImage &image );
	void clearOverlay();
	void hideCardPopup();
	void loadPicture();
	void navigateToPage( ria::qdigidoc4::Pages page, const QStringList &files = QStringList(), bool create = true );
	void onCryptoAction( int code );
	void onSignAction( int code );
	void openFiles( const QStringList files );
	void selectPageIcon( PageIcon* page );
	void showCardMenu( bool show );
	void showOverlay( QWidget *parent );
	void showWarning( const QString &msg, bool isSuccess = false );
	void showWarning( const QString &msg, const QString &details );
	void updateCardData();
	bool validateCardError( QSmartCardData::PinType type, int flags, QSmartCard::ErrorType err );
	QByteArray sendRequest( SSLConnect::RequestType type, const QString &param = QString() );
	void pinUnblock( QSmartCardData::PinType type, bool isForgotPin = false );
	void pinPukChange( QSmartCardData::PinType type );

	Ui::MainWindow *ui;

	std::unique_ptr<CardPopup> cardPopup;
	std::unique_ptr<Overlay> overlay;
	std::unique_ptr<DropdownButton> selector;
	QSmartCard *smartcard = nullptr;
	QButtonGroup *buttonGroup = nullptr;
};
