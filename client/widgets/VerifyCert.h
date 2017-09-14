#ifndef VERIFYCERT_H
#define VERIFYCERT_H

#include <QWidget>
#include <QPainter>

namespace Ui {
class VerifyCert;
}

class VerifyCert : public QWidget
{
	Q_OBJECT

public:
	explicit VerifyCert(QWidget *parent = 0);
	~VerifyCert();

	void update(bool isValid, const QString &name, const QString &validUntil, const QString &change, const QString &forgot_PIN_HTML = "", const QString &details_HTML = "", const QString &error = "");

protected:
	void paintEvent(QPaintEvent *) override;

	void enterEvent(QEvent * event) override;
	void leaveEvent(QEvent * event) override;

private:
	Ui::VerifyCert *ui;
	bool isValid;
};

#endif // VERIFYCERT_H
