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
#include "Styles.h"

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
	parent->installEventFilter(this);
	move(parent->width() - width(), parent->height() - height());

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
		update(ui->actions);
	QWidget::changeEvent(event);
}

void MainAction::hideDropdown()
{
	for(QPushButton *other: ui->list)
		other->deleteLater();
	ui->list.clear();
	setStyleSheet(QStringLiteral("QPushButton { border-top-left-radius: 2px; }"));
}

bool MainAction::eventFilter(QObject *o, QEvent *e)
{
	switch(e->type())
	{
	case QEvent::Resize:
		if(o == parentWidget())
		{
			move(parentWidget()->width() - width(), parentWidget()->height() - height());
			int i = 1;
			for(QPushButton *other: ui->list)
				other->move(pos() + QPoint(0, (-height() - 1) * (i++)));
		}
		break;
	default: break;
	}
	return QWidget::eventFilter(o, e);
}


QString MainAction::label(Actions action) const
{
	switch(action)
	{
	case SignatureMobile: return tr("SignatureMobile");
	case SignatureSmartID: return tr("SignatureSmartID");
	case SignatureToken: return tr("SignatureToken");
	case EncryptContainer: return tr("EncryptContainer");
	case DecryptContainer: return tr("DecryptContainer");
	case DecryptToken: return tr("DECRYPT");
	default: return tr("SignatureAdd");
	}
}

void MainAction::setButtonEnabled(bool enabled)
{
	ui->mainAction->setEnabled(enabled);
}

void MainAction::showDropdown()
{
	if(ui->actions.size() > 1 && ui->list.isEmpty())
	{
		for(QList<Actions>::const_iterator i = ui->actions.cbegin() + 1; i != ui->actions.cend(); ++i)
		{
			QPushButton *other = new QPushButton(label(*i), parentWidget());
			other->setAccessibleName(label(*i).toLower());
			other->resize(size());
			other->move(pos() + QPoint(0, (-height() - 1) * (ui->list.size() + 1)));
			other->show();
			other->setStyleSheet(ui->mainAction->styleSheet() +
				QStringLiteral("\nborder-top-left-radius: 2px; border-top-right-radius: 2px;"));
			other->setFont(ui->mainAction->font());
			connect(other, &QPushButton::clicked, this, &MainAction::hideDropdown);
			connect(other, &QPushButton::clicked, this, [=]{ emit this->action(*i); });
			ui->list.push_back(other);
		}
		setStyleSheet(QStringLiteral("QPushButton { border-top-left-radius: 0px; }"));
	}
	else
		hideDropdown();
}

void MainAction::update(const QList<Actions> &actions)
{
	hideDropdown();
	ui->actions = actions;
	ui->mainAction->setText(label(actions[0]));
	ui->mainAction->setAccessibleName(label(actions[0]).toLower());
	ui->otherCards->setVisible(actions.size() > 1);
	show();
}
