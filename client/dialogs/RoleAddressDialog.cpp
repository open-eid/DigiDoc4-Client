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

#include <QtGui/QValidator>
#include <QtWidgets/QCompleter>
#include <QtWidgets/QListView>

class RoleAddressDialog::Private: public Ui::RoleAddressDialog {};

class SanitizingValidator: public QValidator {
public:
	using QValidator::QValidator;
	State validate(QString &input, int &pos) const override {
		static const QRegularExpression invalid(
			QStringLiteral("[\\x00-\\x08\\x0B\\x0C\\x0E-\\x1F\\x{FFFE}\\x{FFFF}]"));
		int before = input.size();
		input.remove(invalid);
		pos = qMax(0, pos - (before - input.size()));
		return Acceptable;
	}
};

RoleAddressDialog::RoleAddressDialog(QWidget *parent)
	: QDialog(parent)
	, d(new Private)
{
	d->setupUi(this);
#if defined (Q_OS_WIN)
	d->buttonLayout->setDirection(QBoxLayout::RightToLeft);
#endif
	setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint);

	connect( d->cancel, &QPushButton::clicked, this, &RoleAddressDialog::reject );
	connect( d->sign, &QPushButton::clicked, this, &RoleAddressDialog::accept );

	auto *validator = new SanitizingValidator(this);
	auto list = findChildren<QLineEdit*>();
	if(!list.isEmpty())
		list.first()->setFocus();
	for(QLineEdit *line: list)
	{
		line->setAttribute(Qt::WA_MacShowFocusRect, false);
		line->setValidator(validator);
		Settings::Option<QStringList> s{line->objectName(), {}};
		auto *completer = new QCompleter(s, line);
		completer->setMaxVisibleItems(10);
		completer->setCompletionMode(QCompleter::PopupCompletion);
		completer->setCaseSensitivity(Qt::CaseInsensitive);
		line->setText(QStringList(s).value(0));
		line->setCompleter(completer);
		connect(line, &QLineEdit::editingFinished, this, [line, s = std::move(s)] {
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
	new Overlay(this);
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
