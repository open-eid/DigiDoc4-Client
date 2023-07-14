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

#include "PrintSheet.h"

#include "DateTime.h"
#include "DigiDoc.h"
#include "SslCertificate.h"

#include <QtGui/QStaticText>
#include <QtPrintSupport/QPrinter>
#ifdef Q_OS_WIN32
#include <QtCore/qt_windows.h>
#endif

PrintSheet::PrintSheet( DigiDoc *doc, QPrinter *printer )
:	QPainter( printer )
,	p( printer )
{
	printer->setPageOrientation( QPageLayout::Portrait );
	const auto pageRect = p->pageLayout().paintRectPixels(printer->resolution());
	left		= pageRect.left();
	margin		= left;
	right		= pageRect.right() - 2*margin;
	top			= pageRect.top();
	bottom		= pageRect.y() + pageRect.height() - 2*margin;

#ifdef Q_OS_MAC
	scale( 0.8, 0.8 );
	right /= 0.8;
	bottom /= 0.8;
#elif defined(Q_OS_WIN32)
	SetProcessDPIAware();
	HDC screen = GetDC(0);
	qreal dpix = GetDeviceCaps(screen, LOGPIXELSX);
	qreal dpiy = GetDeviceCaps(screen, LOGPIXELSY);
	qreal scaleFactor = dpiy / qreal(96);
	scale(scaleFactor, scaleFactor);
	right /= scaleFactor;
	bottom /= scaleFactor;
	ReleaseDC(NULL, screen);
#endif

	QFont text = font();
	text.setFamily(QStringLiteral("Arial, Liberation Sans, Helvetica, sans-serif"));
	text.setPixelSize( 12 );

	auto fileSize = [](quint64 bytes)
	{
		const quint64 kb = 1024;
		const quint64 mb = 1024 * kb;
		const quint64 gb = 1024 * mb;
		const quint64 tb = 1024 * gb;
		if(bytes >= tb)
			return QStringLiteral("%1 TB").arg(qreal(bytes) / tb, 0, 'f', 3);
		if(bytes >= gb)
			return QStringLiteral("%1 GB").arg(qreal(bytes) / gb, 0, 'f', 2);
		if(bytes >= mb)
			return QStringLiteral("%1 MB").arg(qreal(bytes) / mb, 0, 'f', 1);
		if(bytes >= kb)
			return QStringLiteral("%1 KB").arg(bytes / kb);
		return QStringLiteral("%1 B").arg(bytes);
	};

	QFont head = text;
	QFont sHead = text;
	head.setPixelSize( 28 );
	sHead.setPixelSize( 18 );

	QPen oPen = pen();
	QPen sPen = pen();
	QPen hPen = pen();
	hPen.setWidth( 2 );
	sPen.setWidth( 1 );
	sPen.setStyle( Qt::DotLine );

	setFont( head );
	QRect rect( left, top, right, 60 );
	drawText( rect, Qt::TextWordWrap, tr("VALIDITY CONFIRMATION SHEET"), &rect );
	setPen( hPen );
	drawLine( left, rect.bottom(), right, rect.bottom() );
	top += rect.height() + 30;

	setFont( sHead );
	drawText( left, top, tr("SIGNED FILES") );
	setPen( sPen );
	drawLine( left, top+3, right, top+3 );
	top += 30;

	setFont( text );
	setPen( oPen );
	drawText( left, top, tr("FILE NAME") );
	drawText( right-150, top, tr("FILE SIZE") );
	for( int i = 0; i < doc->documentModel()->rowCount(); ++i )
	{
		int fileHeight = drawTextRect( QRect( left, top+5, right - left - 150, 20 ),
			doc->documentModel()->data(i) );
		drawTextRect( QRect( right-150, top+5, 150, fileHeight ),
			fileSize(doc->documentModel()->fileSize(i)));
		top += fileHeight;
		newPage( 50 );
	}
	top += 35;

	newPage( 50 );
	setFont( sHead );
	drawText( left, top, tr("SIGNERS") );
	setPen( sPen );
	drawLine( left, top+3, right, top+3 );
	top += 30;

	setFont( text );
	setPen( oPen );

	int i = 1;
	for(const DigiDocSignature &sig: doc->signatures())
	{
		const SslCertificate cert = sig.cert();
		bool tempel = cert.type() & SslCertificate::TempelType;

		newPage( 50 );
		drawText( left, top, tr("NO.") );
		drawText( left+40, top, tempel ? tr( "COMPANY" ) : tr( "NAME" ) );
		drawText( right-300, top, tempel ? tr("REGISTER CODE") : tr("PERSONAL CODE") );
		drawText( right-170, top, tr("TIME") );
		top += 5;

		int nameHeight = drawTextRect( QRect( left+40, top, right - left - 340, 20 ),
			cert.toString(cert.showCN() ? QStringLiteral("CN") : QStringLiteral("GN SN")));
		drawTextRect( QRect( left, top, 40, nameHeight ),
			QString::number( i++ ) );
		drawTextRect( QRect( right-300, top, 130, nameHeight ),
			cert.personalCode());
		drawTextRect( QRect( right-170, top, 170, nameHeight ),
			DateTime(sig.trustedTime().toLocalTime()).toStringZ(QStringLiteral("dd.MM.yyyy hh:mm:ss")));
		top += 20 + nameHeight;

		QString valid;
		switch(sig.status())
		{
		case DigiDocSignature::Valid: valid = tr("SIGNATURE IS VALID"); break;
		case DigiDocSignature::Warning:valid = QStringLiteral("%1 (%2)").arg(tr("SIGNATURE IS VALID"), tr("NB! WARNINGS")); break;
		case DigiDocSignature::NonQSCD:valid = QStringLiteral("%1 (%2)").arg(tr("SIGNATURE IS VALID"), tr("NB! RESTRICTIONS")); break;
		case DigiDocSignature::Invalid: valid = tr("SIGNATURE IS NOT VALID") ; break;
		case DigiDocSignature::Unknown: valid = tr("UNKNOWN"); break;
		}
		customText( tr("VALIDITY OF SIGNATURE"), valid );
		customText( tr("ROLE / RESOLUTION"), sig.role() );
		customText( tr("PLACE OF CONFIRMATION (CITY, STATE, ZIP, COUNTRY)"), sig.location() );
		customText( tr("SERIAL NUMBER OF SIGNER CERTIFICATE"), cert.serialNumber() );

		newPage( 50 );
		drawText( left, top, tr("ISSUER OF CERTIFICATE") );
		drawText( left+207, top, tr("AUTHORITY KEY IDENTIFIER") );
		top += 5;
		int issuerHeight = drawTextRect( QRect( left, top, 200, 20 ),
			cert.issuerInfo( QSslCertificate::CommonName ) );
		drawTextRect( QRect( left+200, top, right - left - 200, issuerHeight ),
			SslCertificate::toHex(cert.authorityKeyIdentifier()));
		top += 20 + issuerHeight;

		customText(tr("HASH VALUE OF SIGNATURE"), SslCertificate::toHex(sig.messageImprint()));
		top += 15;
	}
	save();
	newPage( 50 );
	QStaticText textDoc;
	textDoc.setTextFormat(Qt::RichText);
	textDoc.setTextWidth( right - margin );
	textDoc.setText(tr("The print out of files listed in the section <b>\"Signed Files\"</b> "
						"are inseparable part of this Validity Confirmation Sheet."));
	translate( QPoint( left, top - 30) );
	drawStaticText(0, 0, textDoc);
	top += 30;
	restore();

	newPage( 90 );
	drawText( left+3, top, tr("NOTES") );
	top += 10;
	drawRect( left, top, right - margin, 80 );

	top += 95;
	drawText(QRect(left + 5, top + 5,  right - margin -5, 45), Qt::TextWordWrap,
		tr("Presented print summary is informative to confirm existence of signed file with given hash value. "
		"The print summary itself does not have independent verification value. Declaration of signers’ "
		"signature can be verified only through digitally signed file."));

	end();
}

void PrintSheet::customText( const QString &title, const QString &text )
{
	QRect rect( left + 5, top + 5,  right - margin -5, 25 );
	rect.setHeight( std::max( 25,
		fontMetrics().boundingRect( rect, Qt::TextWordWrap|Qt::TextWrapAnywhere, text ).height() ) );

	newPage( 30 + rect.height() );

	drawText( left + 3, top, title );
	rect.moveTop( top + 5 );
	drawText( rect, Qt::TextWordWrap|Qt::TextWrapAnywhere|Qt::AlignVCenter, text );
	drawRect( rect.adjusted( -5, 0, 0, rect.height() > 25 ? 0 : -5 ) );

	top += 20 + rect.height();
}

int PrintSheet::drawTextRect(QRect rect, const QString &text)
{
	QRect result = rect.adjusted( 5, 0, -5, 0 );
	result.setHeight( std::max( rect.height(),
		fontMetrics().boundingRect( result, Qt::TextWordWrap|Qt::TextWrapAnywhere, text ).height() ) );
	drawText( result, Qt::TextWordWrap|Qt::TextWrapAnywhere|Qt::AlignVCenter, text );
	drawRect( result.adjusted( -5, 0, 5, 0 ) );
	return result.height();
}

void PrintSheet::newPage( int height )
{
	if ( top + height > bottom )
	{
		p->newPage();
		top = p->pageLayout().paintRectPixels(p->resolution()).top() + 30;
	}
}
