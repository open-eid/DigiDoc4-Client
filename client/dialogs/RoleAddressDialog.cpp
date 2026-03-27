// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "RoleAddressDialog.h"
#include "ui_RoleAddressDialog.h"

#include "Settings.h"
#include "effects/Overlay.h"

#include <QtWidgets/QCompleter>
#include <QtWidgets/QListView>

class RoleAddressDialog::Private: public Ui::RoleAddressDialog {};

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
