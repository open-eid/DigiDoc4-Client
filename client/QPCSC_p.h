/*
 * QEstEidCommon
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

#include "QPCSC.h"

#include <QtCore/QHash>
#include <QtCore/QMutex>
#include <QtCore/QWaitCondition>

#ifdef Q_OS_WIN
#undef UNICODE
#define NOMINMAX
#include <winsock2.h>
#include <winscard.h>
#elif defined(Q_OS_MAC)
#include <PCSC/wintypes.h>
#include <PCSC/winscard.h>
#include <arpa/inet.h>
#else
#include <wintypes.h>
#include <winscard.h>
#include <arpa/inet.h>
#endif

#ifndef SCARD_CTL_CODE
#define SCARD_CTL_CODE(code) (0x42000000 + (code))
#endif

// http://pcscworkgroup.com/Download/Specifications/pcsc10_v2.02.09.pdf
// http://ludovic.rousseau.free.fr/softwares/pcsc-lite/SecurePIN%20discussion%20v5.pdf
#define CM_IOCTL_GET_FEATURE_REQUEST SCARD_CTL_CODE(3400)

enum DRIVER_FEATURES : quint8 {
FEATURE_VERIFY_PIN_START         = 0x01,
FEATURE_VERIFY_PIN_FINISH        = 0x02,
FEATURE_MODIFY_PIN_START         = 0x03,
FEATURE_MODIFY_PIN_FINISH        = 0x04,
FEATURE_GET_KEY_PRESSED          = 0x05,
FEATURE_VERIFY_PIN_DIRECT        = 0x06,
FEATURE_MODIFY_PIN_DIRECT        = 0x07,
FEATURE_MCT_READER_DIRECT        = 0x08,
FEATURE_MCT_UNIVERSAL            = 0x09,
FEATURE_IFD_PIN_PROPERTIES       = 0x0A,
FEATURE_ABORT                    = 0x0B,
FEATURE_SET_SPE_MESSAGE          = 0x0C,
FEATURE_VERIFY_PIN_DIRECT_APP_ID = 0x0D,
FEATURE_MODIFY_PIN_DIRECT_APP_ID = 0x0E,
FEATURE_WRITE_DISPLAY            = 0x0F,
FEATURE_GET_KEY                  = 0x10,
FEATURE_IFD_DISPLAY_PROPERTIES   = 0x11,
FEATURE_GET_TLV_PROPERTIES       = 0x12,
FEATURE_CCID_ESC_COMMAND         = 0x13
};

#pragma pack(push, 1)

struct PCSC_TLV_STRUCTURE
{
	DRIVER_FEATURES tag;
	quint8 length;
	quint32 value;
};

enum bmFormatString : quint8
{
	FormatBinary = 0, // (1234 => 01h 02h 03h 04h)
	FormatBCD = 1 << 0, // (1234 => 12h 34h)
	FormatASCII = 1 << 1, // (1234 => 31h 32h 33h 34h)
	AlignLeft = 0,
	AlignRight = 1 << 2,
	PINFrameOffsetUnitBits = 0,
	PINFrameOffsetUnitBytes = 1 << 7,
};

enum bmPINBlockString : quint8
{
	PINLengthNone = 0,
	PINFrameSizeAuto = 0,
};

enum bmPINLengthFormat : quint8
{
	PINLengthOffsetUnitBits = 0,
	PINLengthOffsetUnitBytes = 1 << 5,
};

enum bEntryValidationCondition : quint8
{
	ValidOnMaxSizeReached = 1 << 0,
	ValidOnKeyPressed = 1 << 1,
	ValidOnTimeout = 1 << 2,
};

enum bNumberMessage : quint8
{
	NoInvitationMessage = 0,
	OneInvitationMessage = 1,
	TwoInvitationMessage = 2, // MODIFY
	ThreeInvitationMessage = 3, // MODIFY
	CCIDDefaultInvitationMessage = 0xFF,
};

enum bConfirmPIN : quint8
{
	ConfirmNewPin = 1 << 0,
	RequestCurrentPin = 1 << 1,
	AdvancedModify = 1 << 2,
};

struct PIN_VERIFY_STRUCTURE
{
	quint8 bTimerOut; // timeout in seconds (00 means use default timeout)
	quint8 bTimerOut2; // timeout in seconds after first key stroke
	quint8 bmFormatString; // formatting options
	quint8 bmPINBlockString; // PIN block definition
	quint8 bmPINLengthFormat; // PIN length definition
	quint16 wPINMaxExtraDigit; // 0xXXYY where XX is minimum PIN size in digits, and YY is maximum PIN size in digits
	quint8 bEntryValidationCondition; // Conditions under which PIN entry should be considered complete
	quint8 bNumberMessage; // Number of messages to display for PIN verification
	quint16 wLangId; // Language for messages (http://www.usb.org/developers/docs/USB_LANGIDs.pdf)
	quint8 bMsgIndex; // Message index (should be 00)
	quint8 bTeoPrologue[3]; // T=1 I-block prologue field to use (fill with 00)
	quint32 ulDataLength; // length of Data to be sent to the ICC
	quint8 abData[1]; // Data to send to the ICC
};

struct PIN_MODIFY_STRUCTURE
{
	quint8 bTimerOut; // timeout in seconds (00 means use default timeout)
	quint8 bTimerOut2; // timeout in seconds after first key stroke
	quint8 bmFormatString; // formatting options
	quint8 bmPINBlockString; // PIN block definition
	quint8 bmPINLengthFormat; // PIN length definition
	quint8 bInsertionOffsetOld; // Insertion position offset in bytes for the current PIN
	quint8 bInsertionOffsetNew; // Insertion position offset in bytes for the new PIN
	quint16 wPINMaxExtraDigit; // 0xXXYY where XX is minimum PIN size in digits, and YY is maximum PIN size in digits
	quint8 bConfirmPIN; // Flags governing need for confirmation of new PIN
	quint8 bEntryValidationCondition; // Conditions under which PIN entry should be considered complete
	quint8 bNumberMessage; // Number of messages to display for PIN verification
	quint16 wLangId; // Language for messages (http://www.usb.org/developers/docs/USB_LANGIDs.pdf)
	quint8 bMsgIndex1; // Index of the invitation message
	quint8 bMsgIndex2; // Index of the invitation message
	quint8 bMsgIndex3; // Index of the invitation message
	quint8 bTeoPrologue[3]; // T=1 I-block prologue field to use (fill with 00)
	quint32 ulDataLength; // length of Data to be sent to the ICC
	quint8 abData[1]; // Data to send to the ICC
};

struct PIN_PROPERTIES_STRUCTURE
{
	quint16 wLcdLayout;
	quint8 bEntryValidationCondition;
	quint8 bTimeOut2;
};

struct DISPLAY_PROPERTIES_STRUCTURE
{
	quint16 wLcdMaxCharacters;
	quint16 wLcdMaxLines;
};

#pragma pack(pop)

struct QPCSC::Private
{
	SCARDCONTEXT context {};
	SCARDCONTEXT thread {};
	QMutex			sleepMutex;
	QWaitCondition	sleepCond;
};

struct QPCSCReader::Private
{
	QHash<DRIVER_FEATURES,quint32> features();

	QPCSC::Private *d {};
	SCARDHANDLE card {};
	SCARD_IO_REQUEST io {SCARD_PROTOCOL_UNDEFINED, sizeof(SCARD_IO_REQUEST)};
	SCARD_READERSTATE state {};
	QByteArray reader;
	bool isTransacted {};

	QHash<DRIVER_FEATURES,quint32> featuresList;
};
