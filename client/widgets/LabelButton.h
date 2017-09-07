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

#include <QLabel>

// A label that acts as a button.
// LabelButton switches foreground color with background when mouse hovers over it.
class LabelButton : public QLabel
{
    Q_OBJECT

public:
    enum ButtonStyle {
        Mojo = (1 << 0),
        DeepCerulean = (1 << 1),
        WhiteBackground = (1 << 2)
    };

    explicit LabelButton(QWidget *parent = nullptr);

    void init(int style, const QString &label, const QString &url);
    void setStyles( const QString &nStyle, const QString &nLink, const QString &style, const QString &link );
    
protected:
    void enterEvent(QEvent *ev) override;
    void leaveEvent(QEvent *ev) override;

private:
    QString background(int style) const;
    QString foreground(int style) const;
    
    QString normalStyle;
    QString normalLink;
    QString hoverStyle;
    QString hoverLink;

    static const QString styleTemplate;
    static const QString linkTemplate;
};
