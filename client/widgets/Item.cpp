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
