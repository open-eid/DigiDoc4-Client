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

#include "ui_MobileProgress.h"

#include <QtCore/QHash>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QSslError>

class DigiDoc;
class QNetworkAccessManager;
class QNetworkReply;
class QTimeLine;
class QWinTaskbarButton;

class MobileProgress : public QDialog, private Ui::MobileProgress
{
	Q_OBJECT

public:
	explicit MobileProgress( QWidget *parent = 0 );

	void setSignatureInfo( const QString &city, const QString &state,
		const QString &zip, const QString &country, const QString &role );
	void sign( const DigiDoc *doc, const QString &ssid, const QString &cell );
	QByteArray signature() const;
	void stop();

private Q_SLOTS:
	void finished( QNetworkReply *reply );
	void sendStatusRequest();
	void sslErrors( QNetworkReply *reply, const QList<QSslError> &errors );

private:
	void endProgress(const QString &msg);
	static bool isTest( const QString &ssid, const QString &cell );

	QTimeLine *statusTimer;
	QNetworkAccessManager *manager;
	QNetworkRequest request;
	QStringList location;
	QByteArray m_signature;
	QString sessionCode;
	QHash<QString,QString> mobileResults;
	QWinTaskbarButton *taskbar;
};
