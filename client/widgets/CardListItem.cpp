// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "CardListItem.h"
#include "ui_CardListItem.h"

#include "SslCertificate.h"

#include <QtCore/QDateTime>
#include <QtGui/QPainter>
#include <QtWidgets/QStyleOptionButton>

CardListItem::CardListItem(QWidget *parent)
	: QAbstractButton(parent)
	, ui(new Ui::CardListItem)
{
	ui->setupUi(this);
	ui->cardIcon->load(QStringLiteral(":/images/icon_Minu_eID_hover.svg"));
	setAutoExclusive(true);
	connect(this, &QAbstractButton::toggled, this, [this](bool checked) {
		if(checked)
			emit selected(t);
	});
}

CardListItem::~CardListItem()
{
	delete ui;
}

TokenData CardListItem::token() const
{
	return t;
}

void CardListItem::paintEvent(QPaintEvent*)
{
	QStyleOptionButton opt;
	opt.initFrom(this);
	if(isChecked())
		opt.state |= QStyle::State_On;
	if(isDown())
		opt.state |= QStyle::State_Sunken;
	QPainter p(this);
	style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void CardListItem::changeEvent(QEvent *ev)
{
	QAbstractButton::changeEvent(ev);
	if(ev->type() == QEvent::LanguageChange)
	{
		ui->retranslateUi(this);
		update(t, isChecked(), isCheckable());
	}
}

void CardListItem::update(const TokenData &token, bool selected, bool usable)
{
	t = token;
	SslCertificate c(t.cert());
	QString name = !c.subjectInfo("GN").isEmpty() || !c.subjectInfo("SN").isEmpty() ?
		c.toString(QStringLiteral("GN SN")) : c.toString(QStringLiteral("CN"));
	ui->cardName->setText(QStringLiteral("%1, %2").arg(name, c.personalCode()).toHtmlEscaped());
	ui->cardName->setAccessibleName(ui->cardName->text().toLower());
	ui->cardIssuer->setText(QStringLiteral("%1: %2").arg(tr("Issuer"), c.issuerInfo(QSslCertificate::CommonName)));
	ui->cardValidUntil->setText(QStringLiteral("%1: %2").arg(tr("Valid to"),
		c.expiryDate().toLocalTime().toString(QStringLiteral("dd.MM.yyyy"))));

	qint64 leftDays = std::max<qint64>(0, QDateTime::currentDateTime().daysTo(c.expiryDate().toLocalTime()));
	if(!usable || !c.isValid())
		ui->cardValidUntil->setLabel(QStringLiteral("error"));
	else if(leftDays <= 105)
		ui->cardValidUntil->setLabel(QStringLiteral("warning"));
	else
		ui->cardValidUntil->setLabel(QString());

	setCheckable(usable);
	setEnabled(usable);
	setCursor(usable ? Qt::PointingHandCursor : Qt::ArrowCursor);
	setChecked(usable && selected);
}
