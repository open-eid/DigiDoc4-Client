/*
 * QDigiDocClient
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

#include "dialogs/WarningDialog.h"

class QSslCertificate;
class AccessCert: public WarningDialog
{
	Q_OBJECT

public:
	explicit AccessCert(QWidget *parent = nullptr);
	~AccessCert() final;

	bool validate();

	static QSslCertificate cert();
	void increment();
	bool installCert( const QByteArray &data, const QString &password );
	void remove();

private:
	unsigned int count( const QString &date ) const;
	bool isDefaultCert( const QSslCertificate &cert ) const;
	void showWarning( const QString &msg );

	class Private;
	Private *d;
};
