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

#include "Settings.h"
#include "Styles.h"
#include "effects/Overlay.h"

#include <QtWidgets/QCompleter>
#include <QtWidgets/QListView>

class RoleAddressDialog::Private: public Ui::RoleAddressDialog {};

RoleAddressDialog::RoleAddressDialog(QWidget *parent)
	: QDialog(parent)
	, d(new Private)
{
	const QFont regularFont = Styles::font(Styles::Regular, 14);
	const QFont condensed = Styles::font(Styles::Condensed, 14);

	d->setupUi(this);
#if defined (Q_OS_WIN)
	d->horizontalLayout->setDirection(QBoxLayout::RightToLeft);
#endif
	setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint);
	for(QLineEdit *w: findChildren<QLineEdit*>())
		w->setAttribute(Qt::WA_MacShowFocusRect, false);

	connect( d->cancel, &QPushButton::clicked, this, &RoleAddressDialog::reject );
	d->cancel->setFont(condensed);

	connect( d->sign, &QPushButton::clicked, this, &RoleAddressDialog::accept );
	d->sign->setFont(condensed);

	for(auto *label: findChildren<QLabel*>())
		label->setFont(regularFont);

	d->title->setFont(Styles::font(Styles::Regular, 16, QFont::DemiBold));

	auto list = findChildren<QLineEdit*>();
	if(!list.isEmpty())
		list.first()->setFocus();
	for(QLineEdit *line: list)
	{
		Settings::Option<QStringList> s{line->objectName()};
		auto *completer = new QCompleter(s, line);
		completer->setMaxVisibleItems(10);
		completer->setCompletionMode(QCompleter::PopupCompletion);
		completer->setCaseSensitivity(Qt::CaseInsensitive);
		line->setText(QStringList(s).value(0));
		line->setFont(regularFont);
		line->setCompleter(completer);
		connect(line, &QLineEdit::editingFinished, this, [=] {
			QStringList list = s;
			list.removeAll(line->text());
			list.insert(0, line->text());
			if(list.size() > 10)
				list.removeLast();
			s.clear(); // Uses on Windows MULTI_STRING registry
			s = list;
		});
		completer->popup()->setStyleSheet(QStringLiteral("background-color:#FFFFFF; color: #000000;"));
	}
}

RoleAddressDialog::~RoleAddressDialog()
{
	delete d;
}

int RoleAddressDialog::get(QString &city, QString &country, QString &state, QString &zip, QString &role)
{
	if(!Settings::SHOW_ROLE_ADDRESS_INFO)
		return QDialog::Accepted;
	new Overlay(this, parentWidget());
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
