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

#pragma once

#include <memory>

#include <QDialog>
#include <QEvent>
#include <QPainter>


namespace Ui {
class FirstRun;
}

class ArrowKeyFilter : public QObject
{
	Q_OBJECT
protected:
	bool eventFilter(QObject* obj, QEvent* event) override;
};


class FirstRun : public QDialog
{
	Q_OBJECT

public:
	explicit FirstRun(QWidget *parent = 0);
	~FirstRun();

signals:
	void langChanged(const QString& lang);

private:
	enum View
	{
		Language,
		Intro,
		Signing,
		Encryption,
		MyEid
	};

	void showDetails();
	void navigate( bool forward );
	void toPage( View toPage );
	
	Ui::FirstRun *ui;

	View page;

	friend class ArrowKeyFilter;
};
