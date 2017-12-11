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

#include "WarningItem.h"
#include "ui_WarningItem.h"

#include "MainWindow.h"
#include "Styles.h"
#include "VerifyCert.h"


WarningText::WarningText(const QString &text, const QString &details, int page, const QString &property)
: text(text)
, details(details)
, external(false)
, page(page)
, property(property)
{}

WarningText::WarningText(const QString &text, const QString &details, bool external, const QString &property)
: text(text)
, details(details)
, external(external)
, page(-1)
, property(property)
{}

WarningItem::WarningItem(const WarningText &warningText, QWidget *parent)
: StyledWidget(parent)
, ui(new Ui::WarningItem)
, context(warningText.page)
, warnText(warningText)
{
	ui->setupUi(this);
	ui->warningText->setFont(Styles::font(Styles::Regular, 14));
	ui->warningAction->setFont(Styles::font(Styles::Regular, 14, QFont::Bold));

	ui->warningText->setText(warningText.text);
	ui->warningAction->setText(warningText.details);
	ui->warningAction->setOpenExternalLinks(warningText.external);

	if(!warningText.property.isEmpty())
		setProperty(warningText.property.toLatin1(), true);

	connect(ui->warningAction, &QLabel::linkActivated, this, &WarningItem::linkActivated);
}

WarningItem::~WarningItem()
{
	delete ui;
}

bool WarningItem::appearsOnPage(int page) const
{
	return page == context || context == -1;
}

int WarningItem::page() const
{
	return context;
}

void WarningItem::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange)
	{
		ui->retranslateUi(this);

		if( !warnText.property.isEmpty() )
		{
			if( warnText.property.toLatin1() == UNBLOCK_PIN2_WARNING )
			{
				warnText.text = VerifyCert::tr("PIN%1 has been blocked because PIN%1 code has been entered incorrectly 3 times. Unblock to reuse PIN%1.").arg("2");
				warnText.details = QString("<a href='#unblock-PIN2'><span style='color:rgb(53, 55, 57)'>%1</span></a>").arg(VerifyCert::tr("UNBLOCK"));
			}
			else if( warnText.property.toLatin1() == UNBLOCK_PIN1_WARNING )
			{
				warnText.text = VerifyCert::tr("PIN%1 has been blocked because PIN%1 code has been entered incorrectly 3 times. Unblock to reuse PIN%1.").arg("1");
				warnText.details = QString("<a href='#unblock-PIN1'><span style='color:rgb(53, 55, 57)'>%1</span></a>").arg(VerifyCert::tr("UNBLOCK"));
			}
			else if( warnText.property.toLatin1() == UPDATE_CERT_WARNING )
			{
				warnText.text = MainWindow::tr("Card certificates need updating. Updating takes 2-10 minutes and requires a live internet connection. The card must not be removed from the reader before the end of the update.");
				warnText.details = QString("<a href='#update-Certificate'><span style='color:rgb(53, 55, 57)'>%1</span></a>").arg(MainWindow::tr("Update"));
			}
			else if( warnText.property.toLatin1() == CERT_EXPIRED_WARNING )
			{
				warnText.text = tr("Certificates have expired!");
			}
			else if( warnText.property.toLatin1() == CERT_EXPIRY_WARNING )
			{
				warnText.text = tr("Certificates expire soon!");
			}
		}
		ui->warningText->setText(warnText.text);
		ui->warningAction->setText(warnText.details);
	}
	QWidget::changeEvent(event);
}
