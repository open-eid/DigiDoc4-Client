#ifndef OTHERDATA_H
#define OTHERDATA_H

#include <QWidget>
#include <QPainter>

namespace Ui {
class OtherData;
}

class OtherData : public QWidget
{
	Q_OBJECT

public:
	explicit OtherData(QWidget *parent = 0);
	~OtherData();

	void update(bool activate, const QString &eMail = "");

protected:
	void paintEvent(QPaintEvent *) override;

private:
	Ui::OtherData *ui;
};

#endif // OTHERDATA_H
