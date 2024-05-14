#include "PasswordDialog.h"
#include "Crypto.h"
#include "ui_passworddialog.h"

PasswordDialog::PasswordDialog(QWidget *parent)
    : QDialog(parent), mode(Mode::DECRYPT), type(Type::PASSWORD)
    , ui(new Ui::PasswordDialog)
{
    ui->setupUi(this);
    connect(ui->generateKey, &QPushButton::clicked, this, &PasswordDialog::genKeyClicked);
    connect(ui->typeSelector, &QTabWidget::currentChanged, this, &PasswordDialog::typeChanged);
    connect(ui->passwordLine, &QLineEdit::textChanged, this, &PasswordDialog::lineChanged);
    connect(ui->password2Line, &QLineEdit::textChanged, this, &PasswordDialog::lineChanged);
    connect(ui->keyEdit, &QPlainTextEdit::textChanged, this, &PasswordDialog::editChanged);
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
    if (type == Type::PASSWORD) {
        return ui->passwordLine->text().toUtf8();
    } else {
        QString hex = ui->keyEdit->toPlainText();
        return QByteArray::fromHex(hex.toUtf8());
    }
}

void
PasswordDialog::typeChanged(int index)
{
    Type new_type = static_cast<Type>(index);
    if (new_type != type) setMode(mode, new_type);
}

void
PasswordDialog::lineChanged(const QString& text)
{
    updateOK();
}

void
PasswordDialog::editChanged()
{
    updateOK();
}

void
PasswordDialog::genKeyClicked()
{
    QByteArray key = Crypto::random();
    ui->keyEdit->clear();
    ui->keyEdit->appendPlainText(key.toHex());
}

void
PasswordDialog::updateUI()
{
    ui->typeSelector->setCurrentIndex(type);
    if (mode == Mode::DECRYPT) {
        ui->labelLine->setReadOnly(true);
        if (type == Type::PASSWORD) {
            ui->typeSelector->setTabVisible(Type::PASSWORD, true);
            ui->typeSelector->setTabVisible(Type::KEY, false);
            ui->passwordLabel->setText("Enter password to decrypt the document");
            ui->passwordLine->setEchoMode(QLineEdit::EchoMode::Password);
            ui->password2Label->hide();
            ui->password2Line->hide();
        } else {
            ui->typeSelector->setTabVisible(Type::PASSWORD, false);
            ui->typeSelector->setTabVisible(Type::KEY, true);
            ui->keyLabel->setText("Enter key to decrypt the document");
            ui->generateKey->hide();
        }
    } else {
        ui->labelLine->setReadOnly(false);
        if (type == Type::PASSWORD) {
            ui->typeSelector->setTabVisible(Type::PASSWORD, true);
            ui->typeSelector->setTabVisible(Type::KEY, true);
            ui->passwordLabel->setText("Enter a password to encrypt the document");
            ui->passwordLine->setEchoMode(QLineEdit::EchoMode::Password);
            ui->password2Label->show();
            ui->password2Line->show();
        } else {
            ui->typeSelector->setTabVisible(Type::PASSWORD, true);
            ui->typeSelector->setTabVisible(Type::KEY, true);
            ui->keyLabel->setText("Enter a key to encrypt the document");
            ui->generateKey->show();
        }
    }
    updateOK();
}

void
PasswordDialog::updateOK()
{
    bool active = false;
    if (mode == Mode::DECRYPT) {
        if (type == Type::PASSWORD) {
            active = !ui->passwordLine->text().isEmpty();
        } else {
            active = !ui->keyEdit->toPlainText().isEmpty();
        }
    } else {
        if (type == Type::PASSWORD) {
            active = !ui->passwordLine->text().isEmpty() && ui->passwordLine->text() == ui->password2Line->text();
        } else {
            active = !ui->keyEdit->toPlainText().isEmpty();
        }
    }
    ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(active);
}
