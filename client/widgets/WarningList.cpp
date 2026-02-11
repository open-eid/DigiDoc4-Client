#include "WarningList.h"

#include "WarningItem.h"

#include <QtWidgets/QLayout>

WarningList::WarningList(QWidget *parent)
	: StyledWidget(parent)
{}

void WarningList::closeWarnings(int page)
{
	for(auto *warning: findChildren<WarningItem*>())
	{
		if(warning->page() == page)
			delete warning;
	}
}

void WarningList::showWarning(WarningText warningText)
{
	if(!warningText.type)
		return;
	for(auto *warning: findChildren<WarningItem*>())
		if(warning->warningType() == warningText.type)
			return;
	layout()->addWidget(new WarningItem(std::move(warningText), this));
}


void WarningList::updateWarnings(int page)
{
	for(auto *warning: findChildren<WarningItem*>())
		warning->setVisible(warning->page() == page || warning->page() == MainWindow::MyEid);
}
