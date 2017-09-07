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

 #ifndef CONTAINERPAGE_H
#define CONTAINERPAGE_H

#include <QWidget>

namespace Ui {
class ContainerPage;
}

class ContainerPage : public QWidget
{
    Q_OBJECT

public:
    enum ContainerState {
        UnsignedContainer,
        UnsignedSavedContainer,
        SignedContainer,

        UnencryptedContainer,
        EncryptedContainer,
        DecryptedContainer
    };

    explicit ContainerPage(QWidget *parent = 0);
    ~ContainerPage();

    void transition(ContainerState state);

signals:
	void action( const QString &action );

private:
    void init();
    void hideRightPane();
    void showRightPane();

    Ui::ContainerPage *ui;
};

#endif // CONTAINERPAGE_H
