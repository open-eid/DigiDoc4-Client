// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "QCryptoBackend.h"

QString QCryptoBackend::errorString(PinStatus error)
{
	switch( error )
	{
	case PinOK: return QString();
	case PinCanceled: return tr("PIN Canceled");
	case PinLocked: return tr("PIN locked");
	case PinIncorrect: return tr("PIN Incorrect");
	case GeneralError: return tr("PKCS11 general error");
	case DeviceError: return tr("PKCS11 device error");
	default: return tr("Unknown error");
	}
}
