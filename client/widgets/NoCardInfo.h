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

#ifndef NOCARDINFO_H
#define NOCARDINFO_H

#include <QFont>
#include <QSvgWidget>
#include <QWidget>

namespace Ui {
class NoCardInfo;
}

class NoCardInfo : public QWidget
{
    Q_OBJECT

public:
    explicit NoCardInfo( QWidget *parent = 0 );
    ~NoCardInfo();

    void update( const QString &status );

private:
    Ui::NoCardInfo *ui;
    std::unique_ptr<QSvgWidget> cardIcon;
};

#endif // NOCARDINFO_H
