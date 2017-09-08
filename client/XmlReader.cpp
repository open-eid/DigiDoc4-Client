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

#include "XmlReader.h"

#include "MainWindow.h"

#include <QtCore/QHash>

XmlReader::XmlReader( const QByteArray &data ): QXmlStreamReader( data ) {}

QString XmlReader::emailErr( quint8 code )
{
	switch( code )
	{
	case 0: return MainWindow::tr("Success");
	case 1: return MainWindow::tr("ID-card has not been published by locally recognized verification provider.");
	case 2: return MainWindow::tr("Wrong PIN was entered or cancelled, there was a problem with certificates or browser does not support ID-card.");
	case 3: return MainWindow::tr("ID-card certificate is not valid.");
	case 4: return MainWindow::tr("Entrance is permitted only with Estonian personal code.");
	case 10: return MainWindow::tr("Unknown error");
	case 11: return MainWindow::tr("There was an error with request to KMA.");
	case 12: return MainWindow::tr("There was an error with request to Äriregister.");
	case 20: return MainWindow::tr("No official email forwarding addresses was found");
	case 21: return MainWindow::tr("Your email account has been blocked. To open it, please send an email to toimetaja@eesti.ee or call 663 0215.");
	case 22: return MainWindow::tr("Invalid email address");
	case 23: return MainWindow::tr("Forwarding is activated and you have been sent an email with activation key. Forwarding will be activated only after confirming the key.");
	default: return QString();
	};
}

QString XmlReader::mobileErr( quint8 code )
{
	switch( code )
	{
	// Notice
	case 0: return QString();
	case 1: return MainWindow::tr("User has no Mobiil-ID certificates.");
	case 2: return MainWindow::tr("ID-card certificate is not valid.");
	// error
	case 3: return MainWindow::tr("Server could not read or validate ID card certificate!");
	case 100: return MainWindow::tr("Service internal error!");
	case 101: return MainWindow::tr("Mobile interface not ready!");
	default: return QString();
	};
}

QString XmlReader::mobileStatus( const QString &status )
{
	if( status == "Active" ) return MainWindow::tr("certificates are active and Mobiil-ID is usable.");
	if( status == "Not Active" ) return MainWindow::tr("certificates are inactive, to use Mobiil-ID certificates must be activated.");
	if( status == "Suspended" ) return MainWindow::tr("certificates are suspended. To use Mobiil-ID these must be active.");
	if( status == "Revoked" ) return MainWindow::tr("certificates are revoked. To use Mobiil-ID, a new SIM card must be requested from service provider.");
	if( status == "Unknown" ) return MainWindow::tr("certificates status is unknown");
	if( status == "Expired" ) return MainWindow::tr("certificates are expired. New SIM card has to be requested from the Service provider.");
	return QString();
}

Emails XmlReader::readEmailAddresses()
{
	Q_ASSERT( isStartElement() && name() == "ametlik_aadress" );

	Emails emails;
	QString email;
	while( !atEnd() )
	{
		readNext();
		if( !isStartElement() )
			continue;
		if( name() == "epost" )
			email = readElementText();
		else if( name() == "suunamine" )
			emails.insertMulti( email, readForwards() );
	}
	return emails;
}

Emails XmlReader::readEmailStatus( QString &fault )
{
	while( !atEnd() )
	{
		readNext();
		if( !isStartElement() )
			continue;
		if( name() == "fault_code" )
			fault = readElementText();
		else if( name() == "ametlik_aadress" )
			return readEmailAddresses();
	}
	return Emails();
}

Forward XmlReader::readForwards()
{
	Q_ASSERT( isStartElement() && name() == "suunamine" );

	bool emailActive = false, forwardActive = false;
	QString email;

	while( !atEnd() )
	{
		readNext();
		if( isEndElement() )
			break;
		if( !isStartElement() )
			continue;
		if( name() == "epost" )
			email = readElementText();
		else if( name() == "aktiivne" && readElementText() == "true" )
			emailActive = true;
		else if( name() == "aktiiveeritud" && readElementText() == "true" )
			forwardActive = true;
	}
	return Forward( email, emailActive && forwardActive );
}

MobileStatus XmlReader::readMobileStatus( int &faultcode )
{
	MobileStatus result;
	while( !atEnd() )
	{
		readNext();
		if( !isStartElement() )
			continue;
		if( name() == "ResponseStatus" )
			faultcode = readElementText().toInt();
		else if( name() == "MSISDN" || name() == "Operator" || name() == "Status" || name() == "URL" || name() == "MIDCertsValidTo" )
			result[name().toString()] = readElementText();
	}
	return result;
}
