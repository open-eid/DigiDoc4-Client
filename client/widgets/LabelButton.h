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

#include <QLabel>
#include <QSvgWidget>

#include <memory>


// A label with pre-defined styles that acts as a button.
// LabelButton changes style (colors) on hover and when pressed.
class LabelButton : public QLabel
{
    Q_OBJECT

public:
    enum Style {
        BoxedDeepCerulean,
        BoxedMojo,
        BoxedDeepCeruleanWithCuriousBlue, // Edit
        DeepCeruleanWithLochmara, // Add files
        None
    };
    
    explicit LabelButton(QWidget *parent = nullptr);

    void init( Style style, const QString &label, int code );
    void setIcons( const QString &normalIcon, const QString &hoverIcon, const QString &pressedIcon, int x, int y, int w, int h );

signals:
    void clicked(int code);

protected:
    void enterEvent( QEvent *ev ) override;
    void leaveEvent( QEvent *ev ) override;
    bool event( QEvent *ev ) override;

private:
    struct Css {
        QString style;
        QString background;
        QString icon;
    };
    enum State {
        Normal,
        Hover,
        Pressed
    };
    void normal();
    void hover();
    void pressed();
    
    int code;
    int style;
    Css css[3];
    QString normalStyle;
    QString normalLink;
    QString normalIcon;
    QString hoverStyle;
    QString hoverLink;
    QString hoverIcon;
    std::unique_ptr<QSvgWidget> icon;

    static const QString styleTemplate;
    static const QString linkTemplate;
};
