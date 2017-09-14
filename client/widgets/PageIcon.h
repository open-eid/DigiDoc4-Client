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

#ifndef PAGEICON_H
#define PAGEICON_H

#include <QFont>
#include <QPaintEvent>
#include <QString>
#include <QSvgWidget>
#include <QWidget>

namespace Ui {
class PageIcon;
}

class PageIcon : public QWidget
{
    Q_OBJECT

public:
    struct Style
    {
        QFont font;
        QString image, backColor, foreColor;
    };
    explicit PageIcon( QWidget *parent = 0 );
    ~PageIcon();

    void init( const QString &label, const Style &active, const Style &inactive, bool selected );
    void select( bool selected );

signals:
	void activated( PageIcon *const );

protected:
    void mouseReleaseEvent( QMouseEvent *event ) override;
    void paintEvent( QPaintEvent *ev ) override;

private:
    Ui::PageIcon *ui;
    QSvgWidget *icon;
    Style active;
    Style inactive;
    bool selected;

    void updateSelection();
};

#endif // PAGEICON_H
