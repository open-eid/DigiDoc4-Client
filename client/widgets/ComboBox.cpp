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

#include "widgets/ComboBox.h"

#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

ComboBox::ComboBox(QWidget *parent)
	: QComboBox(parent)
{}

void ComboBox::hidePopup()
{
	if(auto *popup = findChild<QWidget*>(QStringLiteral("popup")))
		popup->deleteLater();
}

void ComboBox::showPopup()
{
	auto *popup = new QWidget(this);
	popup->setObjectName(QStringLiteral("popup"));
	popup->setAttribute(Qt::WA_TranslucentBackground);
	popup->setWindowFlags(Qt::Popup|Qt::FramelessWindowHint);
	popup->setMinimumWidth(width());
	popup->move(mapToGlobal(QPoint()));

	auto *c = new QVBoxLayout(popup);
	c->setContentsMargins(0, 0, 0, 0);
	c->setSpacing(0);

	auto *content = new QWidget(popup);
	content->setObjectName(QStringLiteral("content"));
	c->addWidget(content);

	auto *l = new QVBoxLayout(content);
	l->setContentsMargins(0, 0, 0, 0);
	l->setSpacing(0);
	auto addWidget = [=](const QString &text, int index) {
		auto *b = new QPushButton(text, content);
		b->setFont(font());
		b->setMinimumHeight(height());
		connect(b, &QPushButton::clicked, this, [this, index]{ setCurrentIndex(index); hidePopup(); });
		l->addWidget(b);
		return b;
	};
	addWidget(currentText(), currentIndex())->setObjectName(QStringLiteral("selected"));
	for(int i = 0; i < count(); ++i)
	{
		if(i != currentIndex())
			addWidget(itemText(i), i);
	}

	popup->show();
}
