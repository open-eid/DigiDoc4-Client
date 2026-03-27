// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "QPKCS11.h"

#include "pkcs11.h"

#include <QtCore/QLibrary>

#include <vector>

class QPKCS11::Private: public QObject
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
};
