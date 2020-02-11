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

#include <QWidget>

namespace Ui {
class NoCardInfo;
}

class NoCardInfo : public QWidget
{
	Q_OBJECT

public:
	enum Status {
		NoCard,
		NoPCSC,
		NoReader,
		Loading
	};

	explicit NoCardInfo( QWidget *parent = nullptr );
	~NoCardInfo() final;

	void update(Status s);

protected:
	void changeEvent(QEvent* event) final;

private:
	Ui::NoCardInfo *ui;
	Status status = Loading;
};
