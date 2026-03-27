// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "WaitDialog.h"
#include "ui_WaitDialog.h"

#include "effects/Overlay.h"

#include <QTimer>

class WaitDialog : public QDialog
{
	Q_OBJECT

public:
	explicit WaitDialog(QWidget *parent = nullptr);

	void setText(const QString &text);

	static WaitDialog* create(QWidget *parent);
	static void destroy();
	static WaitDialog* instance();

	static WaitDialog *waitDialog;
	Ui::WaitDialog ui;
	QTimer timer;
};

WaitDialog* WaitDialog::waitDialog = nullptr;

WaitDialog::WaitDialog(QWidget *parent)
	: QDialog(parent)
{
	new Overlay(this);
	setWindowFlags(Qt::Dialog|Qt::FramelessWindowHint);
	ui.setupUi(this);
	ui.movie->load(QStringLiteral(":/images/wait.svg"));
	ui.label->setFocusPolicy(Qt::TabFocus);
	move(parent->geometry().center() - geometry().center());
	timer.setSingleShot(true);
	connect(&timer, &QTimer::timeout, this, &WaitDialog::show);
}

WaitDialog* WaitDialog::create(QWidget *parent)
{
	if(!waitDialog)
		waitDialog = new WaitDialog(parent);
	return waitDialog;
}

void WaitDialog::destroy()
{
	delete waitDialog;
	waitDialog = nullptr;
}

WaitDialog* WaitDialog::instance()
{
	return waitDialog;
}

void WaitDialog::setText(const QString &text)
{
	ui.label->setText(text);
}



WaitDialogHolder::WaitDialogHolder(QWidget *parent, const QString &text, bool show)
{
	WaitDialog *dialog = WaitDialog::create(parent);
	dialog->setText(text);
	if(show)
		dialog->show();
	else
		dialog->timer.start(5000);
}

WaitDialogHolder::~WaitDialogHolder()
{
	WaitDialog::destroy();
}



WaitDialogHider::WaitDialogHider()
{
	if(WaitDialog *d = WaitDialog::instance())
	{
		d->timer.stop();
		d->hide();
	}
}

WaitDialogHider::~WaitDialogHider()
{
	if(WaitDialog *d = WaitDialog::instance())
		d->show();
}

#include "WaitDialog.moc"
