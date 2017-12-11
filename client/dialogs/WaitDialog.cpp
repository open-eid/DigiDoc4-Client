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
#include "ui_WaitDialog.h"

#include "Application.h"
#include "Styles.h"
#include "effects/Overlay.h"

#include <QMovie>

WaitHider::WaitHider(WaitDialog *dialog)
: dialog(dialog)
{
	if(dialog)
		dialog->hide();
}

WaitHider::~WaitHider()
{
	if(dialog)
		dialog->show();
}


WaitDialog::WaitDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::WaitDialog)
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

void WaitDialog::closeOverlay()
{
	if(overlay)
		overlay->close();
	delete overlay;
	overlay = nullptr;
}

int WaitDialog::exec()
{
	showOverlay();
	auto rc = QDialog::exec();
	closeOverlay();

	return rc;
}

WaitHider WaitDialog::hider()
{
	WaitDialog *waitDialog = qobject_cast<WaitDialog*>(qApp->activeWindow());
	return WaitHider(waitDialog);
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