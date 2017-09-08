/*
 * QDigiDoc4Common
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

#include "TokenData4.h"

#include <QtCore/QTextStream>
#include "SslCertificate.h"

// Returns company/full name(Given Names + Surname) from ID card
QString TokenData4::GetName() const
{
	QString content;
	QTextStream s( &content );
	SslCertificate c( cert() );

	if( c.type() & SslCertificate::TempelType )
	{
		s << (c.subjectInfo( "O" ).isEmpty() ? c.toString( "CN" ) : c.toString( "O" ));
	}
	else
	{
		if( !c.subjectInfo( "GN" ).isEmpty() && !c.subjectInfo( "SN" ).isEmpty() )
		{
			s << c.toString( "GN" ) << " " << c.toString( "SN" );
		}
		else
		{
			s << c.toString( "CN" );
		}
	}
	return content;
}

// Returns company reg.code/personal code
QString TokenData4::GetCode() const
{
	SslCertificate c( cert() );
	return c.subjectInfo( "serialNumber" );
}
