#ifndef ACCORDIONTITLE_H
#define ACCORDIONTITLE_H

#include <QWidget>
#include <QPainter>


namespace Ui {
class AccordionTitle;
}

class Accordion;

class AccordionTitle : public QWidget
{
	Q_OBJECT

public:
	explicit AccordionTitle(QWidget *parent = 0);
	~AccordionTitle();

	void init(Accordion* accordion, bool open, const QString& caption, QWidget* content);
	void openSection();
	void closeSection();

protected:
	void mouseReleaseEvent(QMouseEvent *event) override;
	void paintEvent(QPaintEvent *) override;

private:
	Ui::AccordionTitle *ui;
	QWidget* content;
	Accordion* accordion;
};

#endif // ACCORDIONTITLE_H
