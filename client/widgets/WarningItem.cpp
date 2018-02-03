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


WarningText::WarningText(const QString &text, const QString &details, int page)
: text(text)
, details(details)
, counter(0)
, external(false)
, page(page)
, warningType(ria::qdigidoc4::NoWarning)
{}

WarningText::WarningText(ria::qdigidoc4::WarningType warningType)
: external(false)
, counter(0)
, page(-1)
, warningType(warningType)
{
}

WarningText::WarningText(ria::qdigidoc4::WarningType warningType, int counter)
: external(true)
, counter(counter)
, page(ria::qdigidoc4::SignDetails)
, warningType(warningType)
{
}

WarningItem::WarningItem(const WarningText &warningText, QWidget *parent)
: StyledWidget(parent)
, ui(new Ui::WarningItem)
, context(warningText.page)
, warnText(warningText)
{
	ui->setupUi(this);
	ui->warningText->setFont(Styles::font(Styles::Regular, 14));
	ui->warningAction->setFont(Styles::font(Styles::Regular, 14, QFont::Bold));

	lookupWarning();
	ui->warningText->setText(warnText.text);
	ui->warningAction->setText(warnText.details);
	ui->warningAction->setOpenExternalLinks(warnText.external);

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

		lookupWarning();
		ui->warningText->setText(warnText.text);
		ui->warningAction->setText(warnText.details);
	}
	QWidget::changeEvent(event);
}

void WarningItem::lookupWarning()
{
	switch(warnText.warningType)
	{
		case ria::qdigidoc4::CertExpiredWarning:
			warnText.text = tr("Certificates have expired!");
			break;
		case ria::qdigidoc4::CertExpiryWarning:
			warnText.text = tr("Certificates expire soon!");
			break;
		case ria::qdigidoc4::UnblockPin1Warning:
			warnText.text = VerifyCert::tr("PIN%1 has been blocked because PIN%1 code has been entered incorrectly 3 times. Unblock to reuse PIN%1.").arg("1");
			warnText.details = QString("<a href='#unblock-PIN1'><span style='color:rgb(53, 55, 57)'>%1</span></a>").arg(VerifyCert::tr("UNBLOCK"));
			break;
		case ria::qdigidoc4::UnblockPin2Warning:
			warnText.text = VerifyCert::tr("PIN%1 has been blocked because PIN%1 code has been entered incorrectly 3 times. Unblock to reuse PIN%1.").arg("2");
			warnText.details = QString("<a href='#unblock-PIN2'><span style='color:rgb(53, 55, 57)'>%1</span></a>").arg(VerifyCert::tr("UNBLOCK"));
			break;
		case ria::qdigidoc4::UpdateCertWarning:
			warnText.text = MainWindow::tr("Card certificates need updating. Updating takes 2-10 minutes and requires a live internet connection. The card must not be removed from the reader before the end of the update.");
			warnText.details = QString("<a href='#update-Certificate'><span style='color:rgb(53, 55, 57)'>%1</span></a>").arg(MainWindow::tr("Update"));
			break;

		case ria::qdigidoc4::InvalidSignatureWarning:
			warnText.text = tr("%n signatures are not valid", "", warnText.counter);
			warnText.details = QString("<a href='%1' style='color: rgb(53, 55, 57)'>%2</a>")
						.arg(tr("https://www.id.ee/index.php?id=30591"), tr("More information"));
			break;
		case ria::qdigidoc4::InvalidTimestampWarning:
			warnText.text = tr("%n timestamps are not valid", "", warnText.counter);
			warnText.details = QString("<a href='%1' style='color: rgb(53, 55, 57)'>%2</a>")
						.arg(tr("https://www.id.ee/index.php?id=30591"), tr("More information"));
			break;
		case ria::qdigidoc4::UnknownSignatureWarning:
			warnText.text = tr("%n signatures are unknown", "", warnText.counter);
			warnText.details = QString("<a href='%1' style='color: rgb(53, 55, 57)'>%2</a>")
						.arg(tr("http://id.ee/?lang=en&id=34317"), tr("More information"));
			break;
		case ria::qdigidoc4::UnknownTimestampWarning:
			warnText.text = tr("%n timestamps are unknown", "", warnText.counter);
			warnText.details = QString("<a href='%1' style='color: rgb(53, 55, 57)'>%2</a>")
						.arg(tr("http://id.ee/?lang=en&id=34317"), tr("More information"));
			break;

		default:
			break;
	}
}

ria::qdigidoc4::WarningType WarningItem::warningType() const
{
	return warnText.warningType;
}
