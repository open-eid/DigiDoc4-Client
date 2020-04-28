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
	if(QWidget *popup = findChild<QWidget*>(QStringLiteral("popup")))
		popup->deleteLater();
}

void ComboBox::showPopup()
{
	QWidget *popup = new QWidget(this);
	popup->setObjectName(QStringLiteral("popup"));
	popup->setWindowFlags(Qt::Popup);
	popup->setMinimumWidth(width());
	popup->move(parentWidget()->mapToGlobal(geometry().topLeft()));

	QVBoxLayout *c = new QVBoxLayout(popup);
	c->setMargin(0);
	c->setSpacing(0);

	QWidget *content = new QWidget(popup);
	content->setObjectName(QStringLiteral("content"));
	c->addWidget(content);

	QVBoxLayout *l = new QVBoxLayout(content);
	l->setMargin(0);
	l->setSpacing(0);
	auto addWidget = [=](const QString &text, int index) {
		QPushButton *b = new QPushButton(text, content);
		b->setFont(font());
		b->setMinimumHeight(minimumHeight());
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
