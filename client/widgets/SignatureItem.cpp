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

#include "SignatureItem.h"
#include "ui_SignatureItem.h"

#include "Styles.h"
#include "dialogs/CertificateDetails.h"
#include "effects/ButtonHoverFilter.h"

#include <common/DateTime.h>
#include <common/SslCertificate.h>

#include <QFontMetrics>
#include <QResizeEvent>
#include <QSvgWidget>
#include <QtCore/QTextStream>

using namespace ria::qdigidoc4;

SignatureItem::SignatureItem(const DigiDocSignature &s, ContainerState state, QWidget *parent)
: StyledWidget(parent)
, ui(new Ui::SignatureItem)
, signature(s)
{
	ui->setupUi(this);
	stateChange(state);

	QFont nameFont(Styles::font( Styles::Regular, 14, QFont::DemiBold));
	QFont statusFont(Styles::font(Styles::Regular, 14));

	ui->name->setFont(nameFont);
	ui->status->setFont(statusFont);
	ui->idSignTime->setFont( Styles::font(Styles::Regular, 11) );
	ui->remove->installEventFilter( new ButtonHoverFilter( ":/images/icon_remove.svg", ":/images/icon_remove_hover.svg", this ) );

	const SslCertificate cert = s.cert();
	QString accessibility, validity, signingInfo, statusText;
	QTextStream sa(&accessibility);
	QTextStream sn(&name);
	QTextStream sc(&validity);
	QTextStream si(&signingInfo);
	
	auto signatureValidity = signature.validate();

	invalid = signatureValidity >= DigiDocSignature::Invalid;
	if(!cert.isNull())
		sn << cert.toString(cert.showCN() ? "CN" : "GN SN").toHtmlEscaped();
	else
		sn << signature.signedBy().toHtmlEscaped();
	sn << " - ";
	ui->name->setText(invalid ? red(name) : name);

	QString label = tr("Signature");
	if(signature.profile() == "TimeStampToken")
	{
		label = tr("Timestamp");
		setIcon(":/images/icon_ajatempel.svg");
	}
	else if(cert.type() & SslCertificate::TempelType)
	{
		setIcon(":/images/icon_digitempel.svg");
	}
	else
	{
		setIcon(":/images/icon_Allkiri_small.svg", 19, 20);
	}
	sa << label << " ";
	switch( signatureValidity )
	{
	case DigiDocSignature::Valid:
		sa << tr("is valid");
		sc << "<font color=\"green\">" << label << " " << tr("is valid") << "</font>";
		break;
	case DigiDocSignature::Warning:
		sa << tr("is valid") << " (" << tr("Warnings") << ")";
		sc << "<font color=\"green\">" << label << " " << tr("is valid") << "</font> <font color=\"gold\">(" << tr("Warnings") << ")";
		break;
	case DigiDocSignature::NonQSCD:
		sa << tr("is valid") << " (" << tr("Restrictions") << ")";
		sc << "<font color=\"green\">" << label << " " << tr("is valid") << "</font> <font color=\"gold\">(" << tr("Restrictions") << ")";
		break;
	case DigiDocSignature::Test:
		sa << tr("is valid") << " (" << tr("Test signature") << ")";
		sc << "<font color=\"green\">" << label << " " << tr("is valid") << "</font> <font>(" << tr("Test signature") << ")";
		break;
	case DigiDocSignature::Invalid:
		sa << tr("is not valid");
		sc << "<font color=\"red\">" << label << " " << tr("is not valid");
		break;
	case DigiDocSignature::Unknown:
		sa << tr("is unknown");
		sc << "<font color=\"red\">" << label << " " << tr("is unknown");
		break;
	}
	ui->status->setText(validity);
	statusText = accessibility;

	if(!cert.isNull())
	{
		auto serial = cert.toString("serialNumber").toHtmlEscaped();
		sa << " " <<  serial << " - ";
		si << serial << " - ";
	}
	DateTime date( signature.dateTime().toLocalTime() );
	if( !date.isNull() )
	{
		sa << " " << tr("Signed on") << " "
			<< date.formatDate( "dd. MMMM yyyy" ) << " "
			<< tr("time") << " "
			<< date.toString( "hh:mm" );
		si << tr("Signed on") << " "
			<< date.formatDate( "dd. MMMM yyyy" ) << " "
			<< tr("time") << " "
			<< date.toString( "hh:mm" );
	}
	ui->idSignTime->setText(signingInfo);

	setAccessibleName(label + " " + cert.toString(cert.showCN() ? "CN" : "GN SN"));
	setAccessibleDescription( accessibility );

	nameMetrics.reset(new QFontMetrics(nameFont));
	QFontMetrics fmStatus(statusFont);
	// Fixed width: status text + signature icon (25px) + remove icon (19px) + 5px margin before remove
	fixedWidth = fmStatus.width(statusText) + 25 + (ui->remove->isHidden() ? 0 : 19 + 5);
	nameWidth = nameMetrics->width(name);
	elideName();
}

SignatureItem::~SignatureItem()
{
	delete ui;
}

void SignatureItem::elideName()
{
	int remaining = width() - fixedWidth;
	if(remaining < nameWidth)
	{
		auto elidedText = nameMetrics->elidedText(name, Qt::ElideMiddle, remaining);
		ui->name->setText(invalid ? red(elidedText) : elidedText);
		elided = true;
	}
	else if(elided)
	{
		ui->name->setText(name);
		ui->name->setText(invalid ? red(name) : name);
		elided = false;
	}
}

void SignatureItem::mouseReleaseEvent(QMouseEvent *event)
{
	CertificateDetails dlg(signature.cert(), this);
	dlg.exec();
}

QString SignatureItem::red(const QString &text)
{
	return "<font color=\"red\">" + text + "</font>";
}

void SignatureItem::resizeEvent(QResizeEvent *event)
{
	if(event->oldSize().width() != event->size().width())
		elideName();
}

void SignatureItem::setIcon(const QString &icon, int width, int height)
{
	auto widget = new QSvgWidget(icon, ui->icon);
	widget->resize(width, height);
	widget->move(0, (this->height() - height) / 2);
	widget->show();
}

void SignatureItem::stateChange(ContainerState state)
{
	if(state == ContainerState::SignedContainer)
	{
		ui->remove->hide();
	}
	else
	{
		ui->remove->show();
	}
}
