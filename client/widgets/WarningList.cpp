#include "WarningList.h"

#include "WarningItem.h"

#include <QtWidgets/QLayout>

using namespace ria::qdigidoc4;

WarningList::WarningList(QWidget *parent)
	: StyledWidget(parent)
{}

void WarningList::clearMyEIDWarnings()
{
	static const QList<int> warningTypes {
		CertExpiredError, CertExpiryWarning,
		UnblockPin1Warning, UnblockPin2Warning,
		ActivatePin2Warning,
	};
	for(auto *warning: findChildren<WarningItem*>())
	{
		if(warningTypes.contains(warning->warningType()))
			warning->deleteLater();
	}
}

void WarningList::closeWarning(int warningType)
{
	for(auto *warning: findChildren<WarningItem*>())
	{
		if(warning->warningType() == warningType)
			warning->deleteLater();
	}
}

void WarningList::closeWarnings(int page)
{
	for(auto *warning: findChildren<WarningItem*>())
	{
		if(warning->page() == page)
			warning->deleteLater();
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
		warning->setVisible(warning->page() == page || warning->page() == -1);
}
