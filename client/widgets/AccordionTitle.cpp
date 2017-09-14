#include "AccordionTitle.h"
#include "ui_AccordionTitle.h"

#include "Accordion.h"


AccordionTitle::AccordionTitle(QWidget *parent) :
	QWidget(parent),
	ui(new Ui::AccordionTitle)
{
	ui->setupUi(this);
}

AccordionTitle::~AccordionTitle()
{
	delete ui;
}

void AccordionTitle::init(Accordion* accordion, bool open, const QString& caption, QWidget* content)
{
	ui->label->setText(caption);
	this->content = content;
	this->accordion = accordion;
	if(open)
		openSection();
	else
		closeSection();
}

void AccordionTitle::openSection()
{
	content->setVisible(true);
	this->setStyleSheet("background-color: #f7f7f7;");
	ui->widget->setStyleSheet("image: url(:/images/accordion_open.png);");
}


void AccordionTitle::closeSection()
{
	content->setVisible(false);
	this->setStyleSheet("background-color: #ffffff;");
	ui->widget->setStyleSheet("image: url(:/images/accordion_closed.png);");
}


void AccordionTitle::mouseReleaseEvent(QMouseEvent *event)
{
	bool willVisible = !content->isVisible();

	if(willVisible)
	{
		content->setVisible(true);
		accordion->closeOtherSection(this);
		openSection();
	}
}

// Needed to setStyleSheet() take effect.
void AccordionTitle::paintEvent(QPaintEvent *)
{
	QStyleOption opt;
	opt.init(this);
	QPainter p(this);
	style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
