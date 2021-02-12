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


#include "WaitDialog.h"
#include "WaitDialog_p.h"
#include "ui_WaitDialog.h"

#include "Application.h"
#include "Styles.h"
#include "effects/Overlay.h"

WaitDialogHider::WaitDialogHider()
{
	if(WaitDialog *d = WaitDialog::instance())
		d->hide();
}

WaitDialogHider::~WaitDialogHider()
{
	if(WaitDialog *d = WaitDialog::instance())
		d->show();
}



WaitDialog* WaitDialog::waitDialog = nullptr;

WaitDialog::WaitDialog(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::WaitDialog)
{
	new Overlay(this, parent);
	setWindowFlags(Qt::Dialog|Qt::FramelessWindowHint);
	ui->setupUi(this);
	ui->movie->load(QStringLiteral(":/images/wait.svg"));
	ui->label->setFont(Styles::font(Styles::Condensed, 24));
	move(parent->geometry().center() - geometry().center());
}

WaitDialog::~WaitDialog()
{
	delete ui;
}

WaitDialog* WaitDialog::create(QWidget *parent)
{
	if(!waitDialog)
		waitDialog = new WaitDialog(parent);
	return waitDialog;
}

void WaitDialog::destroy()
{
	delete waitDialog;
	waitDialog = nullptr;
}

WaitDialog* WaitDialog::instance()
{
	return waitDialog;
}

void WaitDialog::setText(const QString &text)
{
	ui->label->setText(text);
}



WaitDialogHolder::WaitDialogHolder(QWidget *parent, const QString &text)
{
	WaitDialog *dialog = WaitDialog::create(parent);
	dialog->setText(text);
	dialog->show();
}

WaitDialogHolder::~WaitDialogHolder()
{
	close();
}

void WaitDialogHolder::close()
{
	WaitDialog::destroy();
}
