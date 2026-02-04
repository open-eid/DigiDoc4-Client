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

#include "QPKCS11.h"

#include "pkcs11.h"

#include <QtCore/QLibrary>
#include <QtCore/QThread>

#include <vector>

class QPKCS11::Private: public QThread
{
	Q_OBJECT
public:
	QByteArray attribute( CK_SESSION_HANDLE session, CK_OBJECT_HANDLE obj, CK_ATTRIBUTE_TYPE type ) const;
	std::vector<CK_OBJECT_HANDLE> findObject(CK_SESSION_HANDLE session, CK_OBJECT_CLASS cls, const QByteArray &id = {}) const;

	QLibrary		lib;
	CK_FUNCTION_LIST_PTR f = nullptr;
	bool			isFinDriver = false;
	CK_SESSION_HANDLE session = 0;
	QByteArray		id;
	bool			isPSS = false;

	void run() override;
	CK_RV result = CKR_OK;
};
