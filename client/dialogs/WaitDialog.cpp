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
#include "effects/Overlay.h"
#include "Styles.h"
#include <QMovie>

WaitDialog::WaitDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::WaitDialog)
{
	ui->setupUi(this);
	setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
	setWindowModality(Qt::ApplicationModal);

	QMovie *movie = new QMovie(":/images/wait.gif");
	ui->movie->setMovie(movie);
	movie->start();

	ui->label->setFont(Styles::font(Styles::Condensed, 12));
}

WaitDialog::~WaitDialog()
{
	delete ui;
}

int WaitDialog::exec()
{
	Overlay overlay( parentWidget() );
	overlay.show();
	auto rc = QDialog::exec();
	overlay.close();

	return rc;
}
