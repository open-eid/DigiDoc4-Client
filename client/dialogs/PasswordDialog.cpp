#include "PasswordDialog.h"
#include "Crypto.h"
#include "ui_passworddialog.h"

PasswordDialog::PasswordDialog(QWidget *parent)
    : QDialog(parent), mode(Mode::DECRYPT), type(Type::PASSWORD)
    , ui(new Ui::PasswordDialog)
{
    ui->setupUi(this);
    connect(ui->radioPassword, &QRadioButton::toggled, this, &PasswordDialog::passwordSelected);
    connect(ui->radioKey, &QRadioButton::toggled, this, &PasswordDialog::symmetricKeySelected);
    connect(ui->buttonGenKey, &QPushButton::clicked, this, &PasswordDialog::genKeyClicked);
}

PasswordDialog::~PasswordDialog()
{
    delete ui;
}

void
PasswordDialog::setMode(Mode _mode, Type _type)
{
    mode = _mode;
    type = _type;
    updateUI();
}

QString
PasswordDialog::label()
{
    return ui->labelLine->text();
}

QByteArray
PasswordDialog::secret() const
{
    if (type == Type::PASSWORD) {
        return ui->passwordLine->text().toUtf8();
    } else {
        QString hex = ui->passwordLine->text();
        return QByteArray::fromHex(hex.toUtf8());
    }
}

void
PasswordDialog::passwordSelected(bool checked)
{
    Type new_type = (checked) ? Type::PASSWORD : Type::KEY;
    if (new_type != type) setMode(mode, new_type);
}

void
PasswordDialog::symmetricKeySelected(bool checked)
{
    Type new_type = (checked) ? Type::KEY : Type::PASSWORD;
    if (new_type != type) setMode(mode, new_type);
}

void
PasswordDialog::genKeyClicked()
{
    QByteArray key = Crypto::random();
    ui->passwordLine->setText(key.toHex());
}

void
PasswordDialog::updateUI()
{
    if (mode == Mode::DECRYPT) {
        ui->radioBox->hide();
        ui->labelLabel->hide();
        ui->labelLine->hide();
        ui->buttonGenKey->hide();
        if (type == Type::PASSWORD) {
            ui->radioPassword->setChecked(true);
            ui->passwordLabel->setText("Enter password to decrypt the document");
            ui->passwordLine->setEchoMode(QLineEdit::EchoMode::Password);
        } else {
            ui->radioKey->setChecked(true);
            ui->passwordLabel->setText("Enter key to decrypt the document");
            ui->passwordLine->setEchoMode(QLineEdit::EchoMode::Normal);
        }
    } else {
        ui->radioBox->show();
        ui->labelLabel->show();
        ui->labelLine->show();
        if (type == Type::PASSWORD) {
            ui->radioPassword->setChecked(true);
            ui->passwordLabel->setText("Enter a password to encrypt the document");
            ui->passwordLine->setEchoMode(QLineEdit::EchoMode::Password);
            ui->buttonGenKey->hide();
        } else {
            ui->radioKey->setChecked(true);
            ui->passwordLabel->setText("Enter a key to encrypt the document");
            ui->passwordLine->setEchoMode(QLineEdit::EchoMode::Normal);
            ui->buttonGenKey->show();
        }
    }
}
