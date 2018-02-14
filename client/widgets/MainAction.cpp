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


MainAction::MainAction(Actions action, QWidget *parent)
: QWidget(parent)
, ui(new Ui::MainAction)
{
	ui->setupUi(this);
	ui->mainAction->setFont( Styles::font( Styles::Condensed, 16 ) );
	
	connect( ui->mainAction, &QPushButton::clicked, [this](){ emit this->action(this->actionType); });
	connect( ui->otherCards, &QToolButton::clicked, this, &MainAction::dropdown );
	
	update(action);
}

MainAction::~MainAction()
{
	delete ui;
}

void MainAction::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange)
		update();

	QWidget::changeEvent(event);
}

void MainAction::update()
{
	QString label;
	switch(actionType)
	{
	case SignatureMobile:
		label = tr("SignatureMobile");
		break;
	case SignatureToken:
		label = tr("SignatureToken");
		break;
	case EncryptContainer:
		label = tr("EncryptContainer");
		break;
	case DecryptContainer:
		label = tr("DecryptContainer");
		break;
	default:
		label = tr("SignatureAdd");
		break;
	}

	ui->mainAction->setText(label);
	ui->mainAction->show();
}

void MainAction::update(Actions action)
{
	actionType = action;
	update();

	if (action == SignatureAdd || action == SignatureToken) 
		ui->otherCards->show();
	else
		ui->otherCards->hide();
}
