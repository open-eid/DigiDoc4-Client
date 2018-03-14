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

#include <QMovie>

WaitDialogHider::WaitDialogHider()
{
	WaitDialog *d = WaitDialog::instance();
	if(d)
	{
		parent = d->parentWidget();
		text = d->text();
		overlay = d->detachOverlay();
		WaitDialog::destroy();
	}
}

WaitDialogHider::~WaitDialogHider()
{
	if(parent)
	{
		WaitDialog *d = WaitDialog::create(parent, overlay);
		d->setText(text);
		d->open();
	}
}


WaitDialog* WaitDialog::waitDialog = nullptr;

WaitDialog::WaitDialog(QWidget *parent, Overlay *o)
: QDialog(parent)
, ui(new Ui::WaitDialog)
, overlay(o)
{
	ui->setupUi(this);
	setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
	setWindowModality(Qt::WindowModal);

	QMovie *movie = new QMovie(":/images/wait.gif");
	ui->movie->setMovie(movie);
	movie->start();

	ui->label->setFont(Styles::font(Styles::Condensed, 24));
}

WaitDialog::~WaitDialog()
{
	closeOverlay();
	delete ui;
}

WaitDialog* WaitDialog::create(QWidget *parent, Overlay *o)
{
	if(!waitDialog)
		waitDialog = new WaitDialog(parent, o);

	return waitDialog;
}

void WaitDialog::closeOverlay()
{
	if(overlay)
		overlay->close();
	delete overlay;
	overlay = nullptr;
}

Overlay* WaitDialog::detachOverlay()
{
	auto o = overlay;
	overlay = nullptr;
	return o;
}

void WaitDialog::destroy()
{
	if(waitDialog)
	{
		waitDialog->close();
		delete waitDialog;
		waitDialog = nullptr;
	}
}

int WaitDialog::exec()
{
	showOverlay();
	auto rc = QDialog::exec();
	closeOverlay();

	return rc;
}

WaitDialog* WaitDialog::instance()
{
	return waitDialog;
}

void WaitDialog::open()
{
	showOverlay();
	QDialog::open();
}

void WaitDialog::showOverlay()
{
	if(!overlay)
		overlay = new Overlay(parentWidget());
	overlay->show();
}

void WaitDialog::setText(const QString &text)
{
	ui->label->setText(text);
}

QString WaitDialog::text()
{
	return ui->label->text();
}


WaitDialogHolder::WaitDialogHolder(QWidget *parent, const QString& text)
{
	WaitDialog *dialog = WaitDialog::create(parent);
	if(!text.isEmpty())
		dialog->setText(text);
	dialog->open();
}

WaitDialogHolder::~WaitDialogHolder()
{
	WaitDialog::destroy();
}

void WaitDialogHolder::close()
{
	WaitDialog::destroy();
}
