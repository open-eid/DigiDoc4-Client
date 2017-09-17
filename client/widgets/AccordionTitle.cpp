#include "AccordionTitle.h"
#include "ui_AccordionTitle.h"

#include "Styles.h"
#include "widgets/Accordion.h"


AccordionTitle::AccordionTitle(QWidget *parent) :
	StyledWidget(parent),
	ui(new Ui::AccordionTitle)
{
	ui->setupUi(this);
	icon.reset( new QSvgWidget( this ) );
	icon->setStyleSheet( "border: none;" );
	icon->resize( 14, 14 );
	icon->move( 14, 13 );
	ui->label->setFont( Styles::font( Styles::Condensed, 16 ) );
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
	ui->label->setStyleSheet("border: none; color: #006EB5;");
	icon->load( QString( ":/images/dropdown_deep_cerulean.svg" ) );
}


void AccordionTitle::closeSection()
{
	content->setVisible(false);
	ui->label->setStyleSheet("border: none; color: #353739;");
	icon->load( QString( ":/images/dropdown_right.svg" ) );	
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
