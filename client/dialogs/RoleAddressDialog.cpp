/*
 * QDigiDocClient
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

#include "RoleAddressDialog.h"
#include "ui_RoleAddressDialog.h"

#include "effects/Overlay.h"
#include "Settings.h"
#include "Styles.h"

#include <QtWidgets/QCompleter>
#include <QtWidgets/QPushButton>

class RoleAddressDialog::Private: public Ui::RoleAddressDialog
{
public:
	Settings s;
};

RoleAddressDialog::RoleAddressDialog(QWidget *parent)
	: QDialog(parent)
	, d(new Private)
{
	const QFont regularFont = Styles::font(Styles::Regular, 14);
	const QFont buttonFont = Styles::font(Styles::Condensed, 14);

	d->s.beginGroup(QStringLiteral("Client"));
	d->setupUi(this);
	setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint);
	setWindowModality(Qt::ApplicationModal);
	d->buttonBox->addButton(tr("SIGN"), QDialogButtonBox::AcceptRole)->setFont(buttonFont);
	QPushButton *cancel = d->buttonBox->button(QDialogButtonBox::Cancel);
	cancel->setFont(buttonFont);
	cancel->setText(cancel->text().toUpper());

	for(QLabel *label: findChildren<QLabel*>())
		label->setFont(regularFont);
	for(QLineEdit *line: findChildren<QLineEdit*>())
	{
		QCompleter *completer = new QCompleter(d->s.value(line->objectName()).toStringList(), line);
		completer->setMaxVisibleItems(10);
		completer->setCompletionMode(QCompleter::PopupCompletion);
		completer->setCaseSensitivity(Qt::CaseInsensitive);
		line->setText(d->s.value(line->objectName()).toStringList().value(0));
		line->setFont(regularFont);
		line->setCompleter(completer);
		connect(line, &QLineEdit::editingFinished, this, [=] {
			QStringList list = d->s.value(line->objectName()).toStringList();
			list.removeAll(line->text());
			list.insert(0, line->text());
			if(list.size() > 10)
				list.removeLast();
			d->s.setValue(line->objectName(), QString()); // Uses on Windows MULTI_STRING registry
			d->s.setValueEx(line->objectName(), list, QStringList());
		});
	}
}

RoleAddressDialog::~RoleAddressDialog()
{
	delete d;
}

int RoleAddressDialog::get(QString &city, QString &country, QString &state, QString &zip, QString &role)
{
	if(!Settings(qApp->applicationName()).value(QStringLiteral("Client/RoleAddressInfo"), false).toBool())
		return QDialog::Accepted;
	Overlay overlay(parentWidget());
	overlay.show();
	int result = QDialog::exec();
	if(result == QDialog::Rejected)
		return result;
	role = d->Role->text();
	city = d->City->text();
	state = d->State->text();
	country = d->Country->text();
	zip = d->Zip->text();
	return result;
}
