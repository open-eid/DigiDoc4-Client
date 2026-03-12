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
	ui->password2Label->setHidden(mode == Mode::DECRYPT);
	ui->password2Line->setHidden(mode == Mode::DECRYPT);
	if(mode == DECRYPT) {
		ui->passwordLabel->setText(tr("Enter password to decrypt the document"));
		ui->ok->setText(tr("Decrypt"));
	}
	connect(ui->ok, &QPushButton::clicked, this, &QDialog::accept);
	connect(ui->cancel, &QPushButton::clicked, this, &QDialog::reject);
	connect(ui->passwordLine, &QLineEdit::textChanged, this, &PasswordDialog::updateOK);
	connect(ui->password2Line, &QLineEdit::textChanged, this, &PasswordDialog::updateOK);
	updateOK();
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

void
PasswordDialog::updateOK()
{
	bool active = false;
	if(ui->password2Line->isVisible())
	{
		static const QRegularExpression re(QStringLiteral(R"(^(?=.{20,64}$)(?=.*\d)(?=.*[A-Z])(?=.*[a-z]).*$)"));
		active = re.match(ui->passwordLine->text()).hasMatch() &&
			ui->passwordLine->text() == ui->password2Line->text();
	}
	else
		active = !ui->passwordLine->text().isEmpty();
	ui->ok->setEnabled(active);
}
