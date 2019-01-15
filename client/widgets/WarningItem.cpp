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


WarningText::WarningText(QString text, QString details, int page)
	: text(std::move(text))
	, details(std::move(details))
	, page(page)
{}

WarningText::WarningText(ria::qdigidoc4::WarningType warningType, int counter)
	: counter(counter)
	, warningType(warningType)
{}

WarningItem::WarningItem(WarningText warningText, QWidget *parent)
	: StyledWidget(parent)
	, ui(new Ui::WarningItem)
	, warnText(std::move(warningText))
{
	ui->setupUi(this);
	ui->warningText->setFont(Styles::font(Styles::Regular, 14));
	ui->warningAction->setFont(Styles::font(Styles::Regular, 14, QFont::Bold));
	lookupWarning();
	connect(ui->warningAction, &QLabel::linkActivated, this, &WarningItem::linkActivated);
}

WarningItem::~WarningItem()
{
	delete ui;
}

int WarningItem::page() const
{
	return warnText.page;
}

void WarningItem::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange)
	{
		ui->retranslateUi(this);
		lookupWarning();
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
		warnText.text = VerifyCert::tr("PIN%1 has been blocked because PIN%1 code has been entered incorrectly 3 times. Unblock to reuse PIN%1.").arg(1);
		warnText.details = QStringLiteral("<a href='#unblock-PIN1'><span style='color:rgb(53, 55, 57)'>%1</span></a>").arg(VerifyCert::tr("UNBLOCK"));
		break;
	case ria::qdigidoc4::UnblockPin2Warning:
		warnText.text = VerifyCert::tr("PIN%1 has been blocked because PIN%1 code has been entered incorrectly 3 times. Unblock to reuse PIN%1.").arg(2);
		warnText.details = QStringLiteral("<a href='#unblock-PIN2'><span style='color:rgb(53, 55, 57)'>%1</span></a>").arg(VerifyCert::tr("UNBLOCK"));
		break;
	case ria::qdigidoc4::UpdateCertWarning:
		warnText.text = MainWindow::tr("The validity of this document can be extended. The process takes 2−10 minutes and requires an active internet connection. Do not remove the card from the smart card reader until the process is complete.");
		warnText.details = QStringLiteral("<a href='#update-Certificate-%1'><span style='color:rgb(53, 55, 57)'>%2</span></a>").arg(warnText.url, MainWindow::tr("Begin"));
		break;
	// SignDetails
	case ria::qdigidoc4::InvalidSignatureWarning:
		warnText.text = tr("%n signatures are not valid", nullptr, warnText.counter);
		warnText.details = QStringLiteral("<a href='%1' style='color: rgb(53, 55, 57)'>%2</a>")
					.arg(tr("https://www.id.ee/index.php?id=30591"), tr("More information"));
		warnText.page = ria::qdigidoc4::SignDetails;
		ui->warningAction->setOpenExternalLinks(true);
		break;
	case ria::qdigidoc4::InvalidTimestampWarning:
		warnText.text = tr("%n timestamps are not valid", nullptr, warnText.counter);
		warnText.details = QStringLiteral("<a href='%1' style='color: rgb(53, 55, 57)'>%2</a>")
					.arg(tr("https://www.id.ee/index.php?id=30591"), tr("More information"));
		warnText.page = ria::qdigidoc4::SignDetails;
		ui->warningAction->setOpenExternalLinks(true);
		break;
	case ria::qdigidoc4::UnknownSignatureWarning:
		warnText.text = tr("%n signatures are unknown", nullptr, warnText.counter);
		warnText.details = QStringLiteral("<a href='%1' style='color: rgb(53, 55, 57)'>%2</a>")
					.arg(tr("http://id.ee/?lang=en&id=34317"), tr("More information"));
		warnText.page = ria::qdigidoc4::SignDetails;
		ui->warningAction->setOpenExternalLinks(true);
		break;
	case ria::qdigidoc4::UnknownTimestampWarning:
		warnText.text = tr("%n timestamps are unknown", nullptr, warnText.counter);
		warnText.details = QStringLiteral("<a href='%1' style='color: rgb(53, 55, 57)'>%2</a>")
					.arg(tr("http://id.ee/?lang=en&id=34317"), tr("More information"));
		warnText.page = ria::qdigidoc4::SignDetails;
		ui->warningAction->setOpenExternalLinks(true);
		break;
	case ria::qdigidoc4::CheckConnectionWarning:
		warnText.text = MainWindow::tr("Check internet connection");
		warnText.page = ria::qdigidoc4::SignDetails;
		ui->warningAction->setOpenExternalLinks(true);
		break;
	// MyEid
	case ria::qdigidoc4::EmailActivationWarning:
		warnText.text = MainWindow::tr("Failed activating email forwards.");
		warnText.page = ria::qdigidoc4::MyEid;
		break;
	case ria::qdigidoc4::EmailLoadingWarning:
		warnText.text = MainWindow::tr("Failed loading email settings.");
		warnText.page = ria::qdigidoc4::MyEid;
		break;
	case ria::qdigidoc4::PictureLoadingWarning:
		warnText.text = MainWindow::tr("Loading picture failed.");
		//warnText.page = ria::qdigidoc4::MyEid; // Can be loaded multiple pages
		break;
	case ria::qdigidoc4::SSLLoadingWarning:
		warnText.text = MainWindow::tr("Failed to load data");
		warnText.page = ria::qdigidoc4::MyEid;
		break;
	default:
		break;
	}
	ui->warningText->setText(warnText.text);
	ui->warningAction->setText(warnText.details);
}

ria::qdigidoc4::WarningType WarningItem::warningType() const
{
	return warnText.warningType;
}
