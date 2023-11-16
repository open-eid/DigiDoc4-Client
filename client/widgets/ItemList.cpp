/*
 * QDigiDoc4
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "ItemList.h"
#include "ui_ItemList.h"

#include "Styles.h"

#include <QLabel>
#include <QSvgWidget>

using namespace ria::qdigidoc4;


ItemList::ItemList(QWidget *parent)
	: QScrollArea(parent)
	, ui(new Ui::ItemList)
{
	ui->setupUi(this);
	ui->findGroup->hide();
	ui->download->hide();
	ui->count->setFont(Styles::font(Styles::Condensed, 12));
	ui->count->hide();
	ui->infoIcon->hide();
	ui->txtFind->setAttribute(Qt::WA_MacShowFocusRect, false);
	connect(this, &ItemList::idChanged, this, [this](const SslCertificate &cert){ this->cert = cert; });
}

ItemList::~ItemList()
{
	delete ui;
}

void ItemList::addHeader(const char *label)
{
	headerText = label;

	header = new QLabel(tr(label), this);
	header->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	header->setFocusPolicy(Qt::TabFocus);
	header->resize(415, 64);
	header->setFixedHeight(64);
	header->setFont( Styles::font(Styles::Regular, 20));
	header->setStyleSheet(QStringLiteral("border: solid rgba(217, 217, 216, 0.45);"
			"border-width: 0px 0px 1px 0px; color: #041E42;"));
	ui->itemLayout->insertWidget(0, header);
	setTabOrder(this, header);
	setTabOrder(header, ui->header);
}

void ItemList::addHeaderWidget(Item *widget)
{
	addWidget(widget, ui->itemLayout->indexOf(ui->header), header);
	setTabOrder(widget->lastTabWidget(), ui->header);
}

QString ItemList::addLabel()
{
	switch(itemType)
	{
	case ItemFile: return tr("+ ADD MORE FILES");
	case ItemAddress: return tr("+ ADD RECIPIENT");
	case ToAddAdresses: return tr("ADD ALL");
	default: return {};
	}
}

void ItemList::addWidget(Item *widget, int index, QWidget *tabIndex)
{
	if(!tabIndex)
	{
		if(Item *prev = qobject_cast<Item*>(ui->itemLayout->itemAt(index - 1)->widget()))
			tabIndex = prev->lastTabWidget();
		else
			tabIndex = ui->btnFind;
	}
	ui->itemLayout->insertWidget(index, widget);
	connect(widget, &Item::remove, this, &ItemList::remove);
	connect(this, &ItemList::idChanged, widget, &Item::idChanged);
	widget->stateChange(state);
	widget->idChanged(cert);
	widget->show();
	items.push_back(widget);
	widget->initTabOrder(tabIndex);
}

void ItemList::addWidget(Item *widget)
{
	addWidget(widget, ui->itemLayout->indexOf(ui->add));
}

void ItemList::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange)
	{
		ui->retranslateUi(this);

		ui->listHeader->setText(tr(listText));
		ui->txtFind->setPlaceholderText(tr("Enter the personal code, institution or registry code"));
		ui->txtFind->setAccessibleName(ui->txtFind->placeholderText());

		if(header)
			header->setText(tr(headerText));

		ui->add->setText(addLabel());

		if(itemType == ItemAddress)
			setRecipientTooltip();
	}

	QFrame::changeEvent(event);
}

void ItemList::clear()
{
	ui->download->hide();
	ui->count->hide();

	if(header)
		header->deleteLater();
	header = nullptr;

	for(auto it = items.begin(); it != items.end(); it = items.erase(it))
		delete *it;
}

bool ItemList::eventFilter(QObject *o, QEvent *e)
{
	if(o != ui->infoIcon)
		return QScrollArea::eventFilter(o, e);
	switch(e->type())
	{
	case QEvent::Enter:
	case QEvent::Leave:
		ui->infoIcon->load(e->type() == QEvent::Enter ?
			QStringLiteral(":/images/icon_info_hover.svg") : QStringLiteral(":/images/icon_info.svg"));
		return true;
	default: return QScrollArea::eventFilter(o, e);
	}
}

ContainerState ItemList::getState() const { return state; }

int ItemList::index(Item *item) const
{
	return items.indexOf(item);
}

bool ItemList::hasItem(const std::function<bool(Item* const)> &cb)
{
	return std::any_of(items.cbegin(), items.cend(), cb);
}

void ItemList::init(ItemType item, const char *header)
{
	itemType = item;
	ui->listHeader->setText(tr(header));
	ui->listHeader->setAccessibleName(tr(header));
	listText = header;
	ui->listHeader->setFont( Styles::font(Styles::Regular, 20));

	if(item != ToAddAdresses)
	{
		ui->findGroup->hide();
	}
	else
	{
		ui->btnFind->setFont(Styles::font(Styles::Condensed, 14));
		ui->txtFind->setFont(Styles::font(Styles::Regular, 12));
		ui->findGroup->show();
		ui->txtFind->setPlaceholderText(tr("Enter personal code, company or registry code"));
		connect(ui->txtFind, &QLineEdit::returnPressed, this, [this]{ if(!ui->txtFind->text().trimmed().isEmpty()) emit search(ui->txtFind->text()); });
		connect(ui->btnFind, &QPushButton::clicked, this, [this]{ emit search(ui->txtFind->text()); });
		ui->btnFind->setDisabled(ui->txtFind->text().trimmed().isEmpty());
		connect(ui->txtFind, &QLineEdit::textChanged, this, [this](const QString &text){
			const auto isEmpty = text.trimmed().isEmpty();
			ui->btnFind->setDisabled(isEmpty);
			ui->btnFind->setDefault(isEmpty);
			ui->btnFind->setAutoDefault(isEmpty);
		});
	}

	if (itemType == ItemSignature || item == AddedAdresses || this->state == SignedContainer)
	{
		ui->add->hide();
	}
	else
	{
		ui->add->init(LabelButton::DeepCeruleanWithLochmara, addLabel());
		ui->add->setFont(Styles::font(Styles::Condensed, 12));
	}

	if(itemType == ItemAddress)
	{
		ui->add->disconnect();
		ui->infoIcon->load(QStringLiteral(":/images/icon_info.svg"));
		ui->infoIcon->show();
		ui->infoIcon->installEventFilter(this);
		setRecipientTooltip();

		connect(ui->add, &QToolButton::clicked, this, &ItemList::addressSearch);
	}
	else if(itemType == ToAddAdresses)
	{
		ui->add->hide();
		connect(ui->add, &QToolButton::clicked, this, &ItemList::addAll);
	}
}

void ItemList::remove(Item *item)
{
	if(int i = index(item); i != -1)
		emit removed(i);
}

void ItemList::removeItem(int row)
{
	if(row < items.size())
		delete items.takeAt(row);
}

void ItemList::setRecipientTooltip()
{
#ifdef Q_OS_WIN
	// Windows might not show the tooltip correctly (does not fit) in case of tooltip stylesheet;
	// Add empty paragraph in order to avoid cutting the text.
	// See https://bugreports.qt.io/browse/QTBUG-26576
	ui->infoIcon->setToolTip(tr("RECIPIENT_MESSAGE") + "<br />");
#else
	ui->infoIcon->setToolTip(tr("RECIPIENT_MESSAGE"));
#endif
}

void ItemList::stateChange( ContainerState state )
{
	this->state = state;
	ui->add->setVisible(state & (UnsignedContainer | UnsignedSavedContainer | UnencryptedContainer));

	for(auto item: items)
		item->stateChange(state);
}
