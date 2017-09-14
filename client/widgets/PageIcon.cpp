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

#include "PageIcon.h"
#include "ui_PageIcon.h"

#include <QPainter>

PageIcon::PageIcon(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PageIcon)
{
    ui->setupUi(this);

    icon = new QSvgWidget( this );
    icon->resize( 38, 38 );
    icon->move( 30, 16 );
}

PageIcon::~PageIcon()
{
    delete ui;
    delete icon;
}

void PageIcon::init(const QString &label, const Style& active, const Style& inactive, bool selected)
{
    this->active = active;
    this->inactive = inactive;
    
    ui->label->setText( label );
    this->selected = selected;
    updateSelection();
}

void PageIcon::select(bool selected)
{
    this->selected = selected;
    updateSelection();
}

void PageIcon::mouseReleaseEvent(QMouseEvent *event)
{
    if(!selected)
    {
        selected = true;
        updateSelection();
    }
}

void PageIcon::updateSelection()
{
    const Style &style = selected ? active : inactive;
    if (selected)
    {
        emit activated(this);
    }
    
    ui->label->setFont(style.font);
    ui->label->setStyleSheet( QString("background-color: %1; color: %2; border: none;").arg(style.backColor).arg(style.foreColor) );
    icon->load( QString( ":%1" ).arg( style.image ) );
    icon->setStyleSheet(QString("background-color: %1; border: none;").arg(style.backColor));    
    setStyleSheet(QString("background-repeat: none; background-color: %1; border: none;").arg(style.backColor));
}

// Custom widget must override paintEvent in order to use stylesheets
// See https://wiki.qt.io/How_to_Change_the_Background_Color_of_QWidget
void PageIcon::paintEvent(QPaintEvent *ev)
{
    QStyleOption opt;
    opt.init(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
