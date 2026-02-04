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
	connect(ui->mainAction, &QPushButton::clicked, this, [this]{ emit action(ui->actions.value(0)); });
	connect(ui->mainAction, &QPushButton::clicked, this, &MainAction::hideDropdown);
	connect(ui->otherCards, &QToolButton::clicked, this, &MainAction::showDropdown);
	adjustSize();
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
	setStyleSheet(QStringLiteral("QPushButton { border-top-left-radius: 4px; }"));
}

bool MainAction::eventFilter(QObject *watched, QEvent *event)
{
	switch(event->type())
	{
	case QEvent::Resize:
		if(watched == parentWidget())
		{
			move(parentWidget()->width() - width(), parentWidget()->height() - height());
			QWidget* prev = this;
			for(QPushButton *other: std::as_const(ui->list))
			{
				other->move(prev->pos() + QPoint(0, -height() - 1));
				prev = other;
			}
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
	case SignatureMobile: return tr("Sign with\nMobile-ID");
	case SignatureSmartID: return tr("Sign with\nSmart-ID");
	case SignatureToken: return tr("Sign with\nE-Seal");
	case EncryptContainer: return tr("Encrypt");
	case EncryptLT: return tr("Encrypt long-term");
	case DecryptContainer: return tr("Decrypt with\nID-Card");
	case DecryptToken: return tr("Decrypt");
	default: return tr("Sign with\nID-Card");
	}
}

void MainAction::setButtonEnabled(bool enabled)
{
	ui->mainAction->setEnabled(enabled);
}

void MainAction::showActions(QList<Actions> actions)
{
	if(actions.size() == 2 &&
		std::all_of(actions.cbegin(), actions.cend(), [] (Actions action) {
			return action == SignatureMobile || action == SignatureSmartID;
		}) &&
		!Settings::MOBILEID_ORDER)
		std::reverse(actions.begin(), actions.end());
	ui->actions = std::move(actions);
	update();
	ui->otherCards->setVisible(ui->actions.size() > 1);
	show();
}

void MainAction::showDropdown()
{
	if(ui->actions.size() > 1 && ui->list.isEmpty())
	{
		QWidget* prev = this;
		for(auto i = std::next(ui->actions.cbegin()); i != ui->actions.cend(); ++i)
		{
			auto *other = new QPushButton(label(*i), parentWidget());
			other->setCursor(ui->mainAction->cursor());
			other->resize(size());
			other->move(prev->pos() + QPoint(0, -height() - 1));
			prev = other;
			other->show();
			other->setStyleSheet(ui->mainAction->styleSheet() +
				(i + 1 == ui->actions.cend() ? QStringLiteral("\nQPushButton { border-top-left-radius: 4px; }") : QString()));
			connect(other, &QPushButton::clicked, this, [i, this]{
				hideDropdown();
				if (*i == Actions::SignatureMobile)
					Settings::MOBILEID_ORDER = true;
				if (*i == Actions::SignatureSmartID)
					Settings::MOBILEID_ORDER = false;
				emit action(*i);
			});
			ui->list.push_back(other);
		}
		setStyleSheet({});
	}
	else
		hideDropdown();
}

void MainAction::update()
{
	hideDropdown();
	ui->mainAction->setText(label(ui->actions[0]));
}
