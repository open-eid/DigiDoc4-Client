// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "Item.h"

#include <QtCore/QEvent>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>

void Item::idChanged(const SslCertificate & /* cert */) {}
void Item::initTabOrder(QWidget * /* item */) {}
QWidget* Item::lastTabWidget() { return this; }

LabelItem::LabelItem(const char *text, QWidget *parent)
	: Item(parent)
	, _text(text)
{
	setObjectName(QStringLiteral("LabelItem"));
	setStyleSheet(QStringLiteral(R"(
QWidget {
font-family: Roboto, Helvetica;
font-size: 14px;
}
#LabelItem {
border-bottom: 1px solid #E7EAEF;
}
#label {
color: #07142A;
}
)"));
	setLayout(new QVBoxLayout);
	layout()->setContentsMargins(8, 16, 8, 16);
	layout()->addWidget(label = new QLabel(tr(text), this));
	label->setObjectName("label");
	label->setFocusPolicy(Qt::TabFocus);
	label->setWordWrap(true);
}

void LabelItem::changeEvent(QEvent *event)
{
	if (event->type() == QEvent::LanguageChange)
		label->setText(tr(_text));
	Item::changeEvent(event);
}

void LabelItem::initTabOrder(QWidget *item)
{
	setTabOrder(item, label);
	setTabOrder(label, lastTabWidget());
}
