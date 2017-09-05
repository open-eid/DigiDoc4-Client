#include "AccordionTitle.h"
#include "ui_AccordionTitle.h"

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

void AccordionTitle::init(const QString& caption, QWidget* content)
{
    ui->label->setText(caption);
    this->content = content;
    content->setVisible(false);
}

void AccordionTitle::mouseReleaseEvent(QMouseEvent *event)
{
    content->setVisible(!content->isVisible());
}

// Needed to setStyleSheet() take effect.
void AccordionTitle::paintEvent(QPaintEvent *)
{
    QStyleOption opt;
    opt.init(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
