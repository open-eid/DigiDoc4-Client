// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "PasswordDialog.h"
#include "ui_PasswordDialog.h"

#include <QRegularExpression>

PasswordDialog::PasswordDialog(Mode mode, QWidget *parent)
	: QDialog(parent)
	, ui(new Ui::PasswordDialog)
{
	setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint);
	ui->setupUi(this);
	ui->labelLine->setReadOnly(mode == Mode::DECRYPT);
	ui->infoWidget->setHidden(mode == Mode::DECRYPT);
	ui->passwordHint->setHidden(mode == Mode::DECRYPT);
	ui->passwordError->hide();
	ui->password2Label->setHidden(mode == Mode::DECRYPT);
	ui->password2Line->setHidden(mode == Mode::DECRYPT);
	ui->password2Error->hide();
	if(mode == DECRYPT) {
		ui->title->setText(tr("Decrypt with password"));
		ui->passwordLabel->setText(tr("Enter password to decrypt the document"));
		ui->ok->setText(tr("Decrypt"));
		ui->passwordLine->setFocus();
	}
	auto setError = [](LineEdit *input, QLabel *error, const QString &msg) {
		input->setLabel(msg.isEmpty() ? QString() : QStringLiteral("error"));
		error->setFocusPolicy(msg.isEmpty() ? Qt::NoFocus : Qt::TabFocus);
		error->setText(msg);
		error->setHidden(msg.isEmpty());
	};
	connect(ui->passwordLine, &QLineEdit::textEdited, ui->passwordError, [this, setError] {
		setError(ui->passwordLine, ui->passwordError, {});
	});
	connect(ui->password2Line, &QLineEdit::textEdited, ui->password2Error, [this, setError] {
		setError(ui->password2Line, ui->password2Error, {});
	});
	connect(ui->ok, &QPushButton::clicked, this, [this, setError] {
		if(ui->passwordLine->text().isEmpty())
			return setError(ui->passwordLine, ui->passwordError, tr("Password is empty"));
		if(static const QRegularExpression re(QStringLiteral(R"(^(?=.{20,64}$)(?=.*\d)(?=.*[A-Z])(?=.*[a-z]).*$)"));
			ui->password2Line->isVisible() && !re.match(ui->passwordLine->text()).hasMatch())
			return setError(ui->passwordLine, ui->passwordError, tr("Password does not meet complexity requirements"));
		if(ui->password2Line->isVisible() &&
			ui->passwordLine->text() != ui->password2Line->text())
			return setError(ui->password2Line, ui->password2Error, tr("Passwords do not match"));
		accept();
	});
	connect(ui->cancel, &QPushButton::clicked, this, &QDialog::reject);
}

PasswordDialog::~PasswordDialog()
{
	delete ui;
}

void
PasswordDialog::setLabel(const QString& label)
{
	ui->labelLine->setText(label);
}

QString
PasswordDialog::label()
{
	return ui->labelLine->text();
}

QByteArray
PasswordDialog::secret() const
{
	return ui->passwordLine->text().toUtf8();
}
