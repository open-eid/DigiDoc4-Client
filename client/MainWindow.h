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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "QSmartCard.h"
#include "widgets/PageIcon.h"
#include "widgets/AccordionTitle.h"

#include <QWidget>
#include <QButtonGroup>
#include <QMessageBox>

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

private:
    enum Pages {
        SignIntro,
        SignDetails,
        CryptoIntro,
        CryptoDetails,
        MyEid
    };

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

    void navigateToPage( Pages page );
    void onSignAction( int code );
    void onCryptoAction( int code );
    void loadPicture();
    bool validateCardError( QSmartCardData::PinType type, int flags, QSmartCard::ErrorType err );
    void showWarning( const QString &msg );
    void showWarning( const QString &msg, const QString &details );
    
    Ui::MainWindow *ui;

	QSmartCard *smartcard = nullptr;
   	QButtonGroup *buttonGroup = nullptr;
};

#endif // MAINWINDOW_H

