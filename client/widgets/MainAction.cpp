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

#include "MainAction.h"
#include "ui_MainAction.h"
#include "Settings.h"
#include "Styles.h"

#include <QtGui/QPainter>
#include <QtGui/QPainterPath>
#include <QtGui/QPaintEvent>

using namespace ria::qdigidoc4;

class MainAction::Private: public Ui::MainAction
{
public:
	QList<ria::qdigidoc4::Actions> actions;
	QList<QPushButton*> list;
};

MainAction::MainAction(QWidget *parent)
	: QWidget(parent)
	, ui(new Private)
{
	ui->setupUi(this);
	ui->mainAction->setFont(Styles::font(Styles::Condensed, 16));
	ui->otherCards->hide();
	ui->otherCards->installEventFilter(this);
	parent->installEventFilter(this);
	move(parent->width() - width(), parent->height() - height());

	connect(ui->mainAction, &QPushButton::clicked, this, [&]{
		if (ui->actions.value(0) == Actions::SignatureMobile)
			Settings::MOBILEID_ORDER = true;
		if (ui->actions.value(0) == Actions::SignatureSmartID)
			Settings::MOBILEID_ORDER = false;
	});
	connect(ui->mainAction, &QPushButton::clicked, this, [&]{ emit action(ui->actions.value(0)); });
	connect(ui->mainAction, &QPushButton::clicked, this, &MainAction::hideDropdown);
	connect(ui->otherCards, &QToolButton::clicked, this, &MainAction::showDropdown);
}

MainAction::~MainAction()
{
	hideDropdown();
	delete ui;
}

void MainAction::changeEvent(QEvent* event)
{
	if(event->type() == QEvent::LanguageChange)
		update();
	QWidget::changeEvent(event);
}

void MainAction::hideDropdown()
{
	for(QPushButton *other: ui->list)
		other->deleteLater();
	ui->list.clear();
	setStyleSheet(QStringLiteral("QPushButton { border-top-left-radius: 2px; }"));
}

bool MainAction::eventFilter(QObject *watched, QEvent *event)
{
	switch(event->type())
	{
	case QEvent::Resize:
		if(watched == parentWidget())
		{
			move(parentWidget()->width() - width(), parentWidget()->height() - height());
			int i = 1;
			for(QPushButton *other: ui->list)
				other->move(pos() + QPoint(0, (-height() - 1) * (i++)));
		}
		break;
	case QEvent::MouseButtonPress:
		if(watched == ui->otherCards)
			ui->otherCards->setProperty("pressed", true);
		break;
	case QEvent::MouseButtonRelease:
		if(watched == ui->otherCards)
			ui->otherCards->setProperty("pressed", false);
		break;
	case QEvent::Paint:
		if(watched == ui->otherCards)
		{
			auto *button = qobject_cast<QToolButton*>(watched);
			QPainter painter(button);
			painter.setRenderHint(QPainter::Antialiasing);
			if(ui->otherCards->property("pressed").toBool())
				painter.fillRect(button->rect(), QStringLiteral("#41B6E6"));
			else if(button->rect().contains(button->mapFromGlobal(QCursor::pos())))
				painter.fillRect(button->rect(), QStringLiteral("#008DCF"));
			else
				painter.fillRect(button->rect(), QStringLiteral("#006EB5"));
			QRect rect(0, 0, 13, 6);
			rect.moveCenter(button->rect().center());
			painter.setPen(QPen(Qt::white, 2));
			QPainterPath path(rect.bottomLeft());
			path.lineTo(rect.center().x(), rect.top());
			path.lineTo(rect.bottomRight());
			painter.drawPath(path);
			return true;
		}
		break;
	default: break;
	}
	return QWidget::eventFilter(watched, event);
}


QString MainAction::label(Actions action)
{
	switch(action)
	{
	case SignatureMobile: return tr("SignatureMobile");
	case SignatureSmartID: return tr("SignatureSmartID");
	case SignatureToken: return tr("SignatureToken");
	case EncryptContainer: return tr("EncryptContainer");
    case EncryptLT: return tr("EncryptLongTerm");
    case DecryptContainer: return tr("DecryptContainer");
	case DecryptToken: return tr("DECRYPT");
	default: return tr("SignatureAdd");
	}
}

void MainAction::setButtonEnabled(bool enabled)
{
	ui->mainAction->setEnabled(enabled);
}

void MainAction::showActions(const QList<Actions> &actions)
{
	QList<Actions> order = actions;
	if(order.size() == 2 &&
		std::all_of(order.cbegin(), order.cend(), [] (Actions action) {
			return action == SignatureMobile || action == SignatureSmartID;
		}) &&
		!Settings::MOBILEID_ORDER)
	{
		std::reverse(order.begin(), order.end());
	}
	ui->actions = std::move(order);
	update();
	ui->otherCards->setVisible(ui->actions.size() > 1);
	show();
}

void MainAction::showDropdown()
{
	if(ui->actions.size() > 1 && ui->list.isEmpty())
	{
		for(QList<Actions>::const_iterator i = ui->actions.cbegin() + 1; i != ui->actions.cend(); ++i)
		{
			auto *other = new QPushButton(label(*i), parentWidget());
			other->setAccessibleName(label(*i).toLower());
			other->setCursor(ui->mainAction->cursor());
			other->setFont(ui->mainAction->font());
			other->resize(size());
			other->move(pos() + QPoint(0, (-height() - 1) * (ui->list.size() + 1)));
			other->show();
			other->setStyleSheet(ui->mainAction->styleSheet() +
				QStringLiteral("\nborder-top-left-radius: 2px; border-top-right-radius: 2px;"));
			if (*i == Actions::SignatureMobile)
				connect(other, &QPushButton::clicked, this, []{ Settings::MOBILEID_ORDER = true; });
			if (*i == Actions::SignatureSmartID)
				connect(other, &QPushButton::clicked, this, []{ Settings::MOBILEID_ORDER = false; });
			connect(other, &QPushButton::clicked, this, &MainAction::hideDropdown);
			connect(other, &QPushButton::clicked, this, [=]{ emit this->action(*i); });
			ui->list.push_back(other);
		}
		setStyleSheet(QStringLiteral("QPushButton { border-top-left-radius: 0px; }"));
	}
	else
		hideDropdown();
}

void MainAction::update()
{
	hideDropdown();
	ui->mainAction->setText(label(ui->actions[0]));
	ui->mainAction->setAccessibleName(label(ui->actions[0]).toLower());
}
