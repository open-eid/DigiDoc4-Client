// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "CardList.h"
#include "QPCSC.h"

#include <QtCore/QEvent>
#include <QtWidgets/QLayout>
#include <QtWidgets/QScrollBar>

QString CardList::emptyText()
{
	auto &pcsc = QPCSC::instance();
	if(!pcsc.serviceRunning())
		return tr("The PCSC service, required for using the ID-card, is not working. Check your computer settings.");
	const QStringList readers = pcsc.readers();
	return readers.isEmpty() ? tr("No readers found") : tr("No card in reader");
}

QSize CardList::sizeHint() const
{
	QLayout *layout = widget() ? widget()->layout() : nullptr;
	if(!layout)
		return QScrollArea::sizeHint();
	ensurePolished();
	QSize size = widget()->sizeHint();
	int visibleCards = 0;
	for(int i = 0; i < layout->count(); ++i)
	{
		QLayoutItem *item = layout->itemAt(i);
		if(!item->isEmpty() && ++visibleCards > VisibleCards)
			size.rheight() -= item->sizeHint().height() + layout->spacing();
	}
	if(visibleCards > VisibleCards)
		size.rwidth() += verticalScrollBar()->sizeHint().width();
	return size + QSize(2 * frameWidth(), 2 * frameWidth());
}

QSize CardList::minimumSizeHint() const
{
	return sizeHint();
}

bool CardList::eventFilter(QObject *watched, QEvent *event)
{
	if(watched == widget() && event->type() == QEvent::LayoutRequest)
		updateGeometry();
	return QScrollArea::eventFilter(watched, event);
}
