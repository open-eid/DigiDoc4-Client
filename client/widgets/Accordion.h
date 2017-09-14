#ifndef ACCORDION_H
#define ACCORDION_H

#include <QWidget>
#include <QPainter>


namespace Ui {
class Accordion;
}

class AccordionTitle;

class Accordion : public QWidget
{
	Q_OBJECT

public:
	explicit Accordion(QWidget *parent = 0);
	~Accordion();

	void init();
	void closeOtherSection(AccordionTitle* opened);

protected:
	void paintEvent(QPaintEvent *) override;

private:
	Ui::Accordion *ui;
	AccordionTitle* openSection;
};

#endif // ACCORDION_H
