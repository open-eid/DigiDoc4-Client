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

MainAction::MainAction( ria::qdigidoc4::Actions action, const QString& label, QWidget *parent, bool showSelector )
: QWidget(parent)
, ui(new Ui::MainAction)
{
	ui->setupUi(this);
	ui->mainAction->setFont( Styles::font( Styles::Condensed, 16 ) );
	
	connect( ui->mainAction, &QPushButton::clicked, [this](){ emit this->action(this->actionType); });
	connect( ui->otherCards, &QToolButton::clicked, this, &MainAction::dropdown );
	
	update( action, label, showSelector );
}

MainAction::~MainAction()
{
	delete ui;
}

void MainAction::update( ria::qdigidoc4::Actions action, const QString& label, bool showSelector )
{
	actionType = action;
	ui->mainAction->setText( label );
	if ( showSelector ) 
	{
		ui->otherCards->show();
	}
	else
	{
		ui->otherCards->hide();
	}
}