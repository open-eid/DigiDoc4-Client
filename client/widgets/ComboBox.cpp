// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

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
