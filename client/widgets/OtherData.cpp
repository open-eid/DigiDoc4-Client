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

#include "OtherData.h"
#include "ui_OtherData.h"
#include "Styles.h"
#include "XmlReader.h"

OtherData::OtherData(QWidget *parent)
: QWidget(parent)
, ui(new Ui::OtherData)
{
	ui->setupUi(this);

	connect(ui->inputEMail, &QLineEdit::textChanged, this, [this]{ update(true); });
	connect(ui->btnCheckEMail, &QPushButton::clicked, this, &OtherData::checkEMailClicked);
	connect(ui->activate, &QPushButton::clicked, this, &OtherData::activateEMailClicked);

	QFont font = Styles::font( Styles::Regular, 13 );
	QFont condensed = Styles::font( Styles::Condensed, 14 );
	
	ui->label->setFont( Styles::font( Styles::Regular, 13, QFont::DemiBold ) );
	ui->lblEMail->setFont( font );
	ui->lblNoForwarding->setFont( font );
	ui->labelEestiEe->setFont(Styles::font(Styles::Regular, 12));
	ui->activate->setFont( condensed );
	ui->btnCheckEMail->setFont( condensed );
	ui->activate->setStyleSheet(QStringLiteral(
			"padding: 6px 9px;"
			"QPushButton { border-radius: 2px; border: none; color: #ffffff; background-color: #006EB5;}"
			"QPushButton:pressed { border: none; background-color: #006EB5;}"
			"QPushButton:hover:!pressed { background-color: #008DCF;}"
			"QPushButton:disabled { background-color: #BEDBED;};"
			));
}

OtherData::~OtherData()
{
	delete ui;
}

bool OtherData::update(bool activate, const QByteArray &data)
{
	setProperty("cache", data);
	QString eMail;
	quint8 errorCode = 0;
	if(!data.isEmpty())
	{
		QString error;
		Emails emails = XmlReader(data).readEmailStatus(error);
		errorCode = error.toUInt();
		if(emails.isEmpty() || errorCode > 0)
		{
			errorCode = errorCode ? errorCode : 20;
			activate = errorCode == 20 || errorCode == 22;
			eMail = XmlReader::emailErr(errorCode);
		}
		else
		{
			activate = false;
			QStringList text;
			for( Emails::const_iterator i = emails.constBegin(); i != emails.constEnd(); ++i )
			{
				text << QStringLiteral("%1 - %2 (%3)")
					.arg(i.key(), i.value().first, i.value().second ? tr("active") : tr("not active"));
			}
			eMail = text.join(QStringLiteral("<br />"));
		}
	}

	ui->activateEMail->setVisible(activate);
	ui->btnCheckEMail->setHidden(activate || !eMail.isEmpty());
	ui->lblEMail->setHidden(eMail.isEmpty());
	ui->lblEMail->setText(QStringLiteral("<b>%1</b>").arg(eMail));
	if( activate )
	{
		ui->activate->setDisabled(ui->inputEMail->text().isEmpty());
		ui->activate->setCursor(ui->inputEMail->text().isEmpty() ? Qt::ArrowCursor : Qt::PointingHandCursor );
		ui->inputEMail->setFocus();
	}
	else if(!errorCode)
		ui->lblEMail->setText(tr("Your @eesti.ee e-mail has been forwarded to ") + QStringLiteral(" <br/><b>%1</b>").arg(eMail));
	return errorCode == 0;
}

void OtherData::paintEvent(QPaintEvent * /*event*/)
{
	QStyleOption opt;
	opt.init(this);
	QPainter p(this);
	style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

QString OtherData::getEmail()
{
	return ( ui->inputEMail->text().isEmpty() || ui->inputEMail->text().indexOf('@') == -1 ) ? QString() : ui->inputEMail->text();
}

void OtherData::setFocusToEmail()
{
	ui->inputEMail->setFocus();
}

void OtherData::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange)
	{
		ui->retranslateUi(this);
		update(false, property("cache").toByteArray());
	}

	QWidget::changeEvent(event);
}
