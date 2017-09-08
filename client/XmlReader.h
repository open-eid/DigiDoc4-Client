/*
 * QEstEidUtil
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

#include <QtCore/QXmlStreamReader>

template<class Key, class T> class QHash;
template<class Key, class T> class QMultiHash;
template<class A, class B> struct QPair;
typedef QPair<QString,bool> Forward;
typedef QMultiHash<QString,Forward> Emails;
typedef QHash<QString,QString> MobileStatus;

class XmlReader: public QXmlStreamReader
{
public:
	XmlReader( const QByteArray &data );

	Emails readEmailStatus( QString &fault );
	MobileStatus readMobileStatus( int &faultcode );

	static QString emailErr( quint8 code );
	static QString mobileErr( quint8 code );
	static QString mobileStatus( const QString &status );

private:
	Emails readEmailAddresses();
	Forward readForwards();
};
