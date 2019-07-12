#include "WarningList.h"

#include "MainWindow.h"
#include "WarningItem.h"
#include "WarningRibbon.h"

#include "ui_MainWindow.h"

#include <QtWidgets/QBoxLayout>

using namespace ria::qdigidoc4;

WarningList::WarningList(Ui::MainWindow *main, QWidget *parent)
	: QObject(parent)
	, ui(main)
{
	parent->installEventFilter(this);
}

bool WarningList::appearsOnPage(WarningItem *warning, int page) const
{
	return warning->page() == page || warning->page() == -1;
}

void WarningList::clearMyEIDWarnings()
{
	static const QList<int> warningTypes {CertExpiredWarning, CertExpiryWarning, CertRevokedWarning, UnblockPin1Warning, UnblockPin2Warning};
	for(auto warning: warnings)
	{
		if(warningTypes.contains(warning->warningType()) || warning->page() == MyEid)
			closeWarning(warning);
	}
	updateWarnings();
}

void WarningList::closeWarning(int warningType)
{
	for(auto warning: warnings)
	{
		if(warningType == warning->warningType())
			closeWarning(warning);
	}
	updateWarnings();
}

void WarningList::closeWarning(WarningItem *warning)
{
	warnings.removeOne(warning);
	warning->deleteLater();
}

void WarningList::closeWarnings(int page)
{
	for(auto warning: warnings)
	{
		if(warning->page() == page)
			closeWarning(warning);
	}
	updateWarnings();
}

bool WarningList::eventFilter(QObject *object, QEvent *event)
{
	if(object != parent() || event->type() != QEvent::MouseButtonPress)
		return QObject::eventFilter(object, event);

	for(auto warning: warnings)
	{
		if(warning->underMouse())
		{
			closeWarning(warning);
			break;
		}
	}

	if(ribbon && ribbon->underMouse())
	{
		ribbon->flip();
		updateRibbon(ui->startScreen->currentIndex(), ribbon->isExpanded());
	}

	updateWarnings();
	return QObject::eventFilter(object, event);
}

void WarningList::showWarning(const WarningText &warningText)
{
	if(warningText.warningType)
	{
		for(auto warning: warnings)
		{
			if(warning->warningType() == warningText.warningType)
				return;
		}
	}
	WarningItem *warning = new WarningItem(warningText, ui->page);
	auto layout = qobject_cast<QBoxLayout*>(ui->page->layout());
	warnings << warning;
	connect(warning, &WarningItem::linkActivated, this, &WarningList::warningClicked);
	layout->insertWidget(warnings.size(), warning);
	updateWarnings();
}

void WarningList::updateRibbon(int page, bool expanded)
{
	short count = 0;
	for(auto warning: warnings)
	{
		if(appearsOnPage(warning, page))
		{
			warning->setVisible(expanded || count < 3);
			count++;
		}
	}
	if (count > 1)
	{
		QWidget *parentWidget = qobject_cast<QWidget*>(parent());
		QSize sizeBeforeAdjust = parentWidget->size();
		parentWidget->adjustSize();
		QSize sizeAfterAdjust = parentWidget->size();
		// keep the size set-up by user, if possible
		parentWidget->resize(sizeBeforeAdjust.width(), sizeBeforeAdjust.height() > sizeAfterAdjust.height() ? sizeBeforeAdjust.height() : sizeAfterAdjust.height());
	}
}

void WarningList::updateWarnings()
{
	int page = ui->startScreen->currentIndex();
	int count = 0;
	for(auto warning: warnings)
	{
		if(appearsOnPage(warning, page))
			count++;
		else
			warning->hide();
	}

	if(!count)
		ui->topBarShadow->setStyleSheet(QStringLiteral("background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #c8c8c8, stop: 1 #F4F5F6); \nborder: none;"));
	else
		ui->topBarShadow->setStyleSheet(QStringLiteral("background: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #b5aa92, stop: 1 #F8DDA7); \nborder: none;"));

	if(count < 4)
	{
		if(ribbon)
		{
			delete ribbon;
			ribbon = nullptr;
			for(auto warning: warnings)
			{
				if(appearsOnPage(warning, page))
					warning->show();
			}
		}
	}
	else if(!ribbon)
	{
		ribbon = new WarningRibbon(count - 3, ui->page);
		auto layout = qobject_cast<QBoxLayout*>(ui->page->layout());
		layout->insertWidget(warnings.size() + 1, ribbon);
		ribbon->show();
	}
	else
		ribbon->setCount(count - 3);

	updateRibbon(page, !ribbon || ribbon->isExpanded());
}
