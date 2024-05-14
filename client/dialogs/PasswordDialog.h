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

    void setLabel(const QString& label);
    QString label();
    QByteArray secret() const;
private:
    Ui::PasswordDialog *ui;

    void typeChanged(int index);
    void lineChanged(const QString& text);
    void editChanged();
    void genKeyClicked();
    void updateUI();
    void updateOK();
};

#endif // PASSWORDDIALOG_H
