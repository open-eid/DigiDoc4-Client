// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "Label.h"

#include <QEvent>
#include <QLayout>
#include <QStyle>

#include <algorithm>

Label::Label(QWidget *parent)
	: QLabel(parent)
{}

QString Label::label() const
{
	return _label;
}

void Label::setLabel(QString label)
{
	if (label == _label)
		return;
	_label = std::move(label);
	naturalWidth = -1;
	parentWidget()->style()->unpolish(this);
	parentWidget()->style()->polish(this);
	updateFit();
}

bool Label::fitToParentWidth() const
{
	return fitParentWidth;
}

void Label::setFitToParentWidth(bool enabled)
{
	if(fitParentWidth == enabled)
		return;
	fitParentWidth = enabled;
	updateParentEventFilter();
	updateFit();
}

int Label::wrapAtWidth() const
{
	return maximumWrapWidth;
}

void Label::setWrapAtWidth(int width)
{
	width = std::max(0, width);
	if(maximumWrapWidth == width)
		return;
	maximumWrapWidth = width;
	updateParentEventFilter();
	updateFit();
}

void Label::fitToWidth(int availableWidth)
{
	const bool previousWrap = wordWrap();
	if(naturalWidth < 0 || measuredText != text())
	{
		setMinimumWidth(0);
		setMaximumWidth(QWIDGETSIZE_MAX);
		setWordWrap(false);
		ensurePolished();
		naturalWidth = QLabel::sizeHint().width();
		measuredText = text();
	}

	const bool fits = naturalWidth <= availableWidth;
	setMinimumWidth(0);
	setMaximumWidth(fits ? naturalWidth : availableWidth);
	const bool wrap = !fits;
	if(wordWrap() != wrap)
		setWordWrap(wrap);
	if(previousWrap != wrap)
		emit wordWrapChanged(wrap);
}

void Label::changeEvent(QEvent *event)
{
	const bool sizeChanged = event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange;
	if(sizeChanged)
		naturalWidth = -1;
	QLabel::changeEvent(event);
	if(sizeChanged)
		updateFit();
}

bool Label::eventFilter(QObject *watched, QEvent *event)
{
	if(watched == parentWidget() &&
		(event->type() == QEvent::Resize || event->type() == QEvent::LayoutRequest))
		updateFit();
	return QLabel::eventFilter(watched, event);
}

void Label::updateFit()
{
	QWidget *parent = parentWidget();
	QLayout *layout = parent ? parent->layout() : nullptr;
	if(text().isEmpty())
		return;

	int availableWidth = maximumWrapWidth;
	if(fitParentWidth && layout)
	{
		const QMargins margins = layout->contentsMargins();
		const int parentWidth = parent->contentsRect().width() - margins.left() - margins.right();
		availableWidth = availableWidth > 0 ? std::min(availableWidth, parentWidth) : parentWidth;
	}
	if(availableWidth > 0)
		fitToWidth(availableWidth);
}

void Label::updateParentEventFilter()
{
	if(!parentWidget())
		return;
	if(fitParentWidth || maximumWrapWidth > 0)
		parentWidget()->installEventFilter(this);
	else
		parentWidget()->removeEventFilter(this);
}
