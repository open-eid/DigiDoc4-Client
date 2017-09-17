#ifndef ACCORDIONTITLE_H
#define ACCORDIONTITLE_H

#include "widgets/StyledWidget.h"

#include <QWidget>
#include <QPainter>
#include <QSvgWidget>


namespace Ui {
class AccordionTitle;
}

class Accordion;

class AccordionTitle : public StyledWidget
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

private:
	Ui::AccordionTitle *ui;
	std::unique_ptr<QSvgWidget> icon;
	QWidget* content;
	Accordion* accordion;
};

#endif // ACCORDIONTITLE_H
