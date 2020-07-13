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

#include "QCryptoBackend.h"

QCryptoBackend::QCryptoBackend(QObject *parent)
	: QObject(parent)
{}

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
