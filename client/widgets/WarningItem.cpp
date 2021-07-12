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

static QString getLink(const QString& text, const QString& url)
{
	return QStringLiteral("<a href=\"%1\"><span style=\"color: #006EB5; text-decoration: none;\">%2</span></a>.")
			.arg(url, text);
}
void WarningItem::lookupWarning()
{
	switch(warnText.warningType)
	{
	case ria::qdigidoc4::CertExpiredWarning:
		warnText.text = QStringLiteral("%1 %2 %3")
				.arg(tr("Certificates have expired! "),
					tr("You can find instructions on how to get a new document from "),
					 ::getLink(tr("here"),
							   tr("https://www.politsei.ee/en/instructions/applying-for-an-id-card-for-an-adult/")));
		//make a QLabel behave like a hyperlink
		ui->warningText->setTextInteractionFlags(Qt::TextBrowserInteraction);
		ui->warningText->setOpenExternalLinks(true);
		warnText.page = ria::qdigidoc4::MyEid;
		break;
	case ria::qdigidoc4::CertExpiryWarning:
		warnText.text = QStringLiteral("%1 %2 %3")
				.arg(tr("Certificates expire soon! "),
					 tr("You can find instructions on how to get a new document from "),
					 ::getLink(tr("here"),
							   tr("https://www.politsei.ee/en/instructions/applying-for-an-id-card-for-an-adult/")));
		ui->warningText->setTextInteractionFlags(Qt::TextBrowserInteraction);
		ui->warningText->setOpenExternalLinks(true);
		warnText.page = ria::qdigidoc4::MyEid;
		break;
	case ria::qdigidoc4::CertRevokedWarning:
		warnText.text = tr("Certificates are revoked!");
		warnText.details = QStringLiteral("<a href='%1'><span style='color:rgb(53, 55, 57)'>%2</span></a>").arg(tr("https://www.id.ee/en/article/the-majority-of-electronically-used-id-cards-were-renewed/"), tr("Additional information").toUpper());
		break;
	case ria::qdigidoc4::UnblockPin1Warning:
		warnText.text = QStringLiteral("%1 %2").arg(VerifyCert::tr("PIN%1 has been blocked because PIN%1 code has been entered incorrectly 3 times.").arg(1), VerifyCert::tr("Unblock to reuse PIN%1.").arg(1));
		warnText.details = QStringLiteral("<a href='#unblock-PIN1'><span style='color:rgb(53, 55, 57)'>%1</span></a>").arg(VerifyCert::tr("UNBLOCK"));
		ui->warningAction->setOpenExternalLinks(false);
		break;
	case ria::qdigidoc4::UnblockPin2Warning:
		warnText.text = QStringLiteral("%1 %2").arg(VerifyCert::tr("PIN%1 has been blocked because PIN%1 code has been entered incorrectly 3 times.").arg(2), VerifyCert::tr("Unblock to reuse PIN%1.").arg(2));
		warnText.details = QStringLiteral("<a href='#unblock-PIN2'><span style='color:rgb(53, 55, 57)'>%1</span></a>").arg(VerifyCert::tr("UNBLOCK"));
		ui->warningAction->setOpenExternalLinks(false);
		break;
	// SignDetails
	case ria::qdigidoc4::InvalidSignatureWarning:
		warnText.text = tr("%n signatures are not valid", nullptr, warnText.counter);
		warnText.details = QStringLiteral("<a href='%1' style='color: rgb(53, 55, 57)'>%2</a>")
					.arg(tr("https://www.id.ee/en/article/digital-signing-and-electronic-signatures/"), tr("More information"));
		warnText.page = ria::qdigidoc4::SignDetails;
		break;
	case ria::qdigidoc4::InvalidTimestampWarning:
		warnText.text = tr("%n timestamps are not valid", nullptr, warnText.counter);
		warnText.details = QStringLiteral("<a href='%1' style='color: rgb(53, 55, 57)'>%2</a>")
					.arg(tr("https://www.id.ee/en/article/digital-signing-and-electronic-signatures/"), tr("More information"));
		warnText.page = ria::qdigidoc4::SignDetails;
		break;
	case ria::qdigidoc4::UnknownSignatureWarning:
		warnText.text = tr("%n signatures are unknown", nullptr, warnText.counter);
		warnText.details = QStringLiteral("<a href='%1' style='color: rgb(53, 55, 57)'>%2</a>")
					.arg(tr("https://www.id.ee/en/article/digital-signing-and-electronic-signatures/"), tr("More information"));
		warnText.page = ria::qdigidoc4::SignDetails;
		break;
	case ria::qdigidoc4::UnknownTimestampWarning:
		warnText.text = tr("%n timestamps are unknown", nullptr, warnText.counter);
		warnText.details = QStringLiteral("<a href='%1' style='color: rgb(53, 55, 57)'>%2</a>")
					.arg(tr("https://www.id.ee/en/article/digital-signing-and-electronic-signatures/"), tr("More information"));
		warnText.page = ria::qdigidoc4::SignDetails;
		break;
	case ria::qdigidoc4::UnsupportedDDocWarning:
		warnText.text = tr("The current file is a DigiDoc container that is not supported officially any longer. You are not allowed to add or remove signatures to this container.");
		warnText.details = QStringLiteral("<a href='%1' style='color: rgb(53, 55, 57)'>%2</a>")
					.arg(tr("https://www.id.ee/en/article/digidoc-container-format-life-cycle-2/"), tr("More information"));
		warnText.page = ria::qdigidoc4::SignDetails;
		break;
	case ria::qdigidoc4::EmptyFileWarning:
		warnText.text = tr("An empty file is attached to the container. Remove the empty file from the container to sign.");
		warnText.page = ria::qdigidoc4::SignDetails;
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
