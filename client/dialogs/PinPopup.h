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

#include <common/PinDialog.h>
#include <common/TokenData.h>

#include <QDialog>
#include <QtCore/QRegExp>

namespace Ui {
class PinPopup;
}

class PinPopup : public QDialog
{
	Q_OBJECT

public:
	PinPopup( PinDialog::PinFlags flags, const TokenData &t, QWidget *parent = nullptr );
	PinPopup( PinDialog::PinFlags flags, const QSslCertificate &cert, TokenData::TokenFlags token, QWidget *parent = nullptr );
	PinPopup( PinDialog::PinFlags flags, const QString &title, TokenData::TokenFlags token, QWidget *parent = nullptr, const QString &bodyText = "" );

	~PinPopup();

    int exec() override;
    QString text() const;

signals:
	void startTimer();

private:
    void init( PinDialog::PinFlags flags, const QString &title, TokenData::TokenFlags token, const QString &bodyText=""  );
    void textEdited( const QString &text );

	Ui::PinPopup *ui;
	QRegExp		regexp;
};

