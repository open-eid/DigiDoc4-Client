#ifndef PASSWORDDIALOG_H
#define PASSWORDDIALOG_H

#include <QDialog>

namespace Ui {
class PasswordDialog;
}

class PasswordDialog : public QDialog
{
    Q_OBJECT

public:
    enum Mode {
        ENCRYPT,
        DECRYPT
    };

    enum Type {
        PASSWORD,
        KEY
    };

    Mode mode;
    Type type;

    explicit PasswordDialog(QWidget *parent = nullptr);
    ~PasswordDialog();

    void setMode(Mode mode, Type type);

    QString label();
    QByteArray secret() const;
private:
    Ui::PasswordDialog *ui;

    void passwordSelected(bool checked);
    void symmetricKeySelected(bool checked);
    void genKeyClicked();
    void updateUI();
};

#endif // PASSWORDDIALOG_H
