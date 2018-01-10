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

#include "Application.h"
#include "Styles.h"
#include "dialogs/SignatureDialog.h"
#include "dialogs/WarningDialog.h"

#include <common/DateTime.h>
#include <common/SslCertificate.h>

#include <QFontMetrics>
#include <QResizeEvent>
#include <QSvgWidget>
#include <QtCore/QTextStream>

using namespace ria::qdigidoc4;

#define ICON_WIDTH 19
#define ITEM_HEIGHT 44
#define LINE_HEIGHT 16


SignatureItem::SignatureItem(const DigiDocSignature &s, ContainerState state, bool isSupported, QWidget *parent)
: Item(parent)
, ui(new Ui::SignatureItem)
, signature(s)
{
	ui->setupUi(this);

	QFont nameFont(Styles::font(Styles::Regular, 14, QFont::DemiBold));
	nameMetrics.reset(new QFontMetrics(nameFont));	

	ui->name->setFont(nameFont);
	ui->idSignTime->setFont( Styles::font(Styles::Regular, 11) );
	ui->remove->setIcons("/images/icon_remove.svg", "/images/icon_remove_hover.svg", "/images/icon_remove_pressed.svg", 1, 1, 17, 17);
	ui->remove->init(LabelButton::White, "", 0);
	ui->remove->setVisible(isSupported);
	connect(ui->remove, &LabelButton::clicked, this, &SignatureItem::removeSignature);
	init();
}

SignatureItem::~SignatureItem()
{
	delete ui;
}

void SignatureItem::init()
{
	const SslCertificate cert = signature.cert();

	QString accessibility, signingInfo;
	name = QString();
	serial = QString();
	status = QString();
	statusHtml = QString();

	QTextStream sa(&accessibility);
	QTextStream sc(&statusHtml);
	QTextStream si(&signingInfo);
	
	auto signatureValidity = signature.validate();

	invalid = signatureValidity >= DigiDocSignature::Invalid;
	if(!cert.isNull())
		name = cert.toString(cert.showCN() ? "CN" : "GN SN").toHtmlEscaped();
	else
		name = signature.signedBy().toHtmlEscaped();

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
		setIcon(":/images/icon_Allkiri_small.svg", ICON_WIDTH, 20);
	}
	sa << label << " ";
	sc << "<span style=\"font-weight:normal;\">";
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
	sc << "</span>";
	ui->name->setText((invalid ? red(name + " - ") : name + " - ") + statusHtml);
	status = accessibility;

	if(!cert.isNull())
	{
		serial = cert.toString("serialNumber").toHtmlEscaped();
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

	recalculate();
	changeNameHeight();
}

void SignatureItem::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange)
	{
		ui->retranslateUi(this);
		init();
	}

	QWidget::changeEvent(event);
}

void SignatureItem::changeNameHeight()
{
	if((width() - reservedWidth) < nameWidth)
	{
		ui->name->setMinimumHeight(LINE_HEIGHT * 2);
		ui->name->setMaximumHeight(LINE_HEIGHT * 2);
		setMinimumHeight(ITEM_HEIGHT + LINE_HEIGHT);
		setMaximumHeight(ITEM_HEIGHT + LINE_HEIGHT);
		ui->name->setText((invalid ? red(name) : name) + "<br/>" + statusHtml);
		enlarged = true;
	}
	else if(enlarged)
	{
		ui->name->setMinimumHeight(LINE_HEIGHT);
		ui->name->setMaximumHeight(LINE_HEIGHT);
		setMinimumHeight(ITEM_HEIGHT);
		setMaximumHeight(ITEM_HEIGHT);
		ui->name->setText((invalid ? red(name + " - ") : name + " - ") + statusHtml);
		enlarged = false;
	}
}

void SignatureItem::details()
{
	SignatureDialog dlg(signature, this);
	dlg.exec();
}

QString SignatureItem::getName() const
{
	return name;
}

QString SignatureItem::getStatus() const
{
	return status;
}

QString SignatureItem::id() const
{
	return signature.id();
}

bool SignatureItem::isInvalid() const
{
	return invalid;
}

bool SignatureItem::isSelfSigned(const QString& cardCode, const QString& mobileCode) const
{
	return serial == cardCode || serial == mobileCode;
}

void SignatureItem::mouseReleaseEvent(QMouseEvent *event)
{
	details();
}

void SignatureItem::recalculate()
{
	// Reserved width: signature icon (24px) + remove icon (19px) + 5px margin before remove
	reservedWidth = ICON_WIDTH + 5 + (ui->remove->isHidden() ? 0 : ICON_WIDTH + 5);
	nameWidth = nameMetrics->width(name  + " - " + status);
}

QString SignatureItem::red(const QString &text)
{
	return "<font color=\"red\">" + text + "</font>";
}

void SignatureItem::removeSignature()
{
	const SslCertificate c = signature.cert();
	QString msg = tr("Remove signature %1")
		.arg( c.toString( c.showCN() ? "CN serialNumber" : "GN SN serialNumber" ) );

	WarningDialog dlg(msg, qApp->activeWindow());
	dlg.setCancelText(tr("CANCEL"));
	dlg.addButton(tr("OK"), SignatureRemove);
	dlg.exec();
	if(dlg.result() == SignatureRemove)
		emit remove(this);
}

void SignatureItem::resizeEvent(QResizeEvent *event)
{
	if(event->oldSize().width() != event->size().width())
		changeNameHeight();
}

void SignatureItem::setIcon(const QString &icon, int width, int height)
{
	auto widget = new QSvgWidget(icon, ui->icon);
	widget->resize(width, height);
	widget->move(0, (this->height() - height) / 2);
	widget->show();
}
