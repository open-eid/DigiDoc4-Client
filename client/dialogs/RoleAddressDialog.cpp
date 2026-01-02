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
#include "effects/Overlay.h"

#include <QtWidgets/QCompleter>
#include <QtWidgets/QListView>

class RoleAddressDialog::Private: public Ui::RoleAddressDialog {};

static QString cleanUp(const QString& src) {
	QString dst(src.size(), QChar(0));
	size_t dlen = 0;
	for (auto s = src.cbegin(); s != src.cend(); s++) {
		if ((*s <= ' ') &&  (*s != QChar(0x9)) && (*s != QChar(0xa)) && (*s != QChar(0xd))) continue;
		if ((*s == QChar(0xfffe)) || (*s == QChar(0xffff))) continue;
		dst[dlen++] = *s;
	}
	dst.resize(dlen);
	return dst;
}

RoleAddressDialog::RoleAddressDialog(QWidget *parent)
	: QDialog(parent)
	, d(new Private)
{
	d->setupUi(this);
#if defined (Q_OS_WIN)
	d->buttonLayout->setDirection(QBoxLayout::RightToLeft);
#endif
	setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint);
	for(QLineEdit *w: findChildren<QLineEdit*>())
		w->setAttribute(Qt::WA_MacShowFocusRect, false);

	connect( d->cancel, &QPushButton::clicked, this, &RoleAddressDialog::reject );
	connect( d->sign, &QPushButton::clicked, this, &RoleAddressDialog::accept );

	auto list = findChildren<QLineEdit*>();
	if(!list.isEmpty())
		list.first()->setFocus();
	for(QLineEdit *line: list)
	{
		Settings::Option<QStringList> s{line->objectName(), {}};
		auto *completer = new QCompleter(s, line);
		completer->setMaxVisibleItems(10);
		completer->setCompletionMode(QCompleter::PopupCompletion);
		completer->setCaseSensitivity(Qt::CaseInsensitive);
		line->setText(QStringList(s).value(0));
		line->setCompleter(completer);
		connect(line, &QLineEdit::editingFinished, this, [line, s = std::move(s)] {
			QStringList list = s;
			QString text = cleanUp(line->text());
			list.removeAll(text);
			list.insert(0, text);
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
	new Overlay(this);
	int result = QDialog::exec();
	if(result == QDialog::Rejected)
		return result;
	role = cleanUp(d->Role->text());
	city = cleanUp(d->City->text());
	state = cleanUp(d->State->text());
	country = cleanUp(d->Country->text());
	zip = cleanUp(d->Zip->text());
	return result;
}
