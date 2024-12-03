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

WaitDialog* WaitDialog::waitDialog = nullptr;

WaitDialog::WaitDialog(QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::WaitDialog)
{
	new Overlay(this);
	setWindowFlags(Qt::Dialog|Qt::FramelessWindowHint);
	ui->setupUi(this);
	ui->movie->load(QStringLiteral(":/images/wait.svg"));
	ui->label->setFont(Styles::font(Styles::Condensed, 24));
	ui->label->setFocusPolicy(Qt::TabFocus);
	move(parent->geometry().center() - geometry().center());
	timer.setSingleShot(true);
	connect(&timer, &QTimer::timeout, this, &WaitDialog::show);
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



WaitDialogHolder::WaitDialogHolder(QWidget *parent, const QString &text, bool show)
{
	WaitDialog *dialog = WaitDialog::create(parent);
	dialog->setText(text);
	if(show)
		dialog->show();
	else
		dialog->timer.start(5000);
}

WaitDialogHolder::~WaitDialogHolder()
{
	WaitDialog::destroy();
}



WaitDialogHider::WaitDialogHider()
{
	if(WaitDialog *d = WaitDialog::instance())
	{
		d->timer.stop();
		d->hide();
	}
}

WaitDialogHider::~WaitDialogHider()
{
	if(WaitDialog *d = WaitDialog::instance())
		d->show();
}
