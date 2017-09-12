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

#ifndef CARDINFO_H
#define CARDINFO_H

#include <QFont>
#include <QWidget>

namespace Ui {
class CardInfo;
}

class CardInfo : public QWidget
{
    Q_OBJECT

public:
    explicit CardInfo( QWidget *parent = 0 );
    ~CardInfo();

    void showPicture( const QPixmap &pix );
    void update( const QString &name, const QString &code, const QString &status );

signals:
    void thePhotoLabelClicked();

private Q_SLOTS:
    void thePhotoLabelHasBeenClicked( int code );

private:
    Ui::CardInfo *ui;
};

#endif // CARDINFO_H
