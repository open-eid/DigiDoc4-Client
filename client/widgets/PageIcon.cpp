#include "PageIcon.h"
#include "ui_PageIcon.h"

#include <QDebug>
#include <QPainter>

PageIcon::PageIcon(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PageIcon)
{
    ui->setupUi(this);
}

PageIcon::~PageIcon()
{
    delete ui;
}

void PageIcon::init(const QString &label, const Style& active, const Style& inactive, bool selected)
{
    this->active = active;
    this->inactive = inactive;
    this->selected = selected;
    ui->label->setText( label );
    updateSelection();
}

void PageIcon::mouseReleaseEvent(QMouseEvent *event)
{
    selected = !selected;
    updateSelection();
}

void PageIcon::updateSelection()
{
    const Style &style = selected ? active : inactive;
    
    qDebug() << "set background " << style.backColor;
    ui->label->setFont(style.font);
    ui->label->setStyleSheet( QString("background-color: %1; color: %2; border: none;").arg(style.backColor).arg(style.foreColor) );
    ui->icon->setStyleSheet(QString("background-repeat: none; background-image: url(:%1); background-color: %2; border: none;")
        .arg(style.image).arg(style.backColor));
    // setAutoFillBackground(true);
    // setPalette(QPalette(QColor(style.backColor)));
    setStyleSheet(QString("background-repeat: none; background-color: %1; border: none;").arg(style.backColor));
}

// Custom widget must override painEvent in order to use stylesheets
// See https://wiki.qt.io/How_to_Change_the_Background_Color_of_QWidget
void PageIcon::paintEvent(QPaintEvent *ev)
{
    QStyleOption opt;
    opt.init(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}
