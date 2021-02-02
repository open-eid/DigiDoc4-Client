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
	tabIndex = ui->btnFind;

	connect(this, &ItemList::idChanged, this, [this](const SslCertificate &cert){ this->cert = cert; });
}

ItemList::~ItemList()
{
	delete ui;
}

void ItemList::addHeader(const QString &label)
{
	headerText = label;

	header = new QLabel(tr(qPrintable(label)), this);
	header->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
	header->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	header->resize(415, 64);
	header->setFixedHeight(64);
	header->setFont( Styles::font(Styles::Regular, 20));
	header->setStyleSheet(QStringLiteral("border: solid rgba(217, 217, 216, 0.45);"
			"border-width: 0px 0px 1px 0px; color: #041E42;"));
	ui->itemLayout->insertWidget(0, header);
	headerItems++;
}

void ItemList::addHeaderWidget(Item *widget)
{
	addWidget(widget, headerItems - 1);
	headerItems++;
}

void ItemList::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange)
	{
		tr("Add recipients");
		tr("Added recipients");
		tr("Recipients");
		tr("Encrypted files");
		tr("Container is not signed");
		tr("Content of the envelope");
		tr("Container's signatures");
		tr("Container's timestamps");
		tr("Container's files");
		ui->retranslateUi(this);

		ui->listHeader->setText(tr(qPrintable(listText)));
		ui->txtFind->setPlaceholderText(tr("Enter the personal code, institution or registry code"));
		ui->txtFind->setAccessibleName(ui->txtFind->placeholderText());

		if(header)
			header->setText(tr(qPrintable(headerText)));

		ui->add->setText(addLabel());

		if(itemType == ItemAddress)
			setRecipientTooltip();
	}

	QFrame::changeEvent(event);
}

QString ItemList::addLabel()
{
	switch(itemType)
	{
	case ItemFile: return tr("+ ADD MORE FILES");
	case ItemAddress: return tr("+ ADD RECIPIENT");
	case ToAddAdresses: return tr("ADD ALL");
	default: return QString();
	}
}

void ItemList::addWidget(Item *widget, int index)
{
	ui->itemLayout->insertWidget(index, widget);
	connect(widget, &Item::remove, this, &ItemList::remove);
	connect(this, &ItemList::idChanged, widget, &Item::idChanged);
	widget->stateChange(state);
	widget->idChanged(cert);
	widget->show();
	items.push_back(widget);
	tabIndex = widget->initTabOrder(tabIndex);
}

void ItemList::addWidget(Item *widget)
{
	addWidget(widget, items.size() + headerItems);
}

void ItemList::clear()
{
	ui->download->hide();
	ui->count->hide();
	tabIndex = ui->btnFind;

	if(header)
	{
		header->close();
		delete header;
		header = nullptr;
		headerItems = ui->findGroup->isHidden() ? 1 : 2;
	}

	Item* widget;
	auto it = items.begin();
	while (it != items.end()) {
		widget = *it;
		it = items.erase(it);
		widget->close();
		delete widget;
	}
}

void ItemList::details(const QString &id)
{
	for(auto item: items)
	{
		if(item->id() == id)
			emit item->details();
	}
}

bool ItemList::eventFilter(QObject *o, QEvent *e)
{
	if(o != ui->infoIcon)
		return QFrame::eventFilter(o, e);
	switch(e->type())
	{
	case QEvent::Enter:
	case QEvent::Leave:
		infoIcon->setHidden(e->type() == QEvent::Enter);
		infoHoverIcon->setVisible(e->type() == QEvent::Enter);
		return true;
	default: return QFrame::eventFilter(o, e);
	}
}

ContainerState ItemList::getState() const { return state; }

int ItemList::index(Item *item) const
{
	auto it = std::find(items.begin(), items.end(), item);
	if(it != items.end())
		return std::distance(items.begin(), it);

	return -1;
}

bool ItemList::hasItem(std::function<bool(Item* const)> cb)
{
	for(auto item: items)
	{
		if(cb(item))
			return true;
	}

	return false;
}

void ItemList::init(ItemType item, const QString &header)
{
	itemType = item;
	ui->listHeader->setText(tr(qPrintable(header)));
	ui->listHeader->setAccessibleName(tr(qPrintable(header)));
	listText = header;
	ui->listHeader->setFont( Styles::font(Styles::Regular, 20));

	if(item != ToAddAdresses)
	{
		ui->findGroup->hide();
		headerItems = 1;
	}
	else
	{
		ui->btnFind->setFont(Styles::font(Styles::Condensed, 14));
		ui->txtFind->setFont(Styles::font(Styles::Regular, 12));
		ui->findGroup->show();
		ui->txtFind->setPlaceholderText(tr("Enter personal code, company or registry code"));
		connect(ui->txtFind, &QLineEdit::returnPressed, this, [this]{ if(!ui->txtFind->text().isEmpty()) emit search(ui->txtFind->text()); });
		connect(ui->btnFind, &QPushButton::clicked, this, [this]{ emit search(ui->txtFind->text()); });
		ui->btnFind->setDisabled(ui->txtFind->text().isEmpty());
		connect(ui->txtFind, &QLineEdit::textChanged, this, [this](const QString &text){
			ui->btnFind->setDisabled(text.isEmpty());
			ui->btnFind->setDefault(text.isEmpty());
			ui->btnFind->setAutoDefault(text.isEmpty());
		});
		headerItems = 2;
	}

	if (itemType == ItemSignature || item == AddedAdresses || this->state == SignedContainer)
	{
		ui->add->hide();
	}
	else
	{
		ui->add->init(LabelButton::DeepCeruleanWithLochmara, addLabel(), itemType == ItemFile ? FileAdd : AddressAdd);
		ui->add->setFont(Styles::font(Styles::Condensed, 12));
	}

	if(itemType == ItemAddress)
	{
		ui->add->disconnect();
		infoIcon = new QSvgWidget(ui->infoIcon);
		infoIcon->load(QStringLiteral(":/images/icon_info.svg"));
		infoIcon->resize(15, 15);
		infoIcon->move(1, 1);
		infoIcon->show();
		infoHoverIcon = new QSvgWidget(ui->infoIcon);
		infoHoverIcon->hide();
		infoHoverIcon->load(QStringLiteral(":/images/icon_info_hover.svg"));
		infoHoverIcon->resize(15, 15);
		infoHoverIcon->move(1, 1);
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
	int i = index(item);
	if(i != -1)
		emit removed(i);
}

void ItemList::removeItem(int row)
{
	if(items.size() < row)
		return;

	auto item = items[row];
	item->close();
	items.erase(items.begin()+row);
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

void ItemList::setTerm(const QString &term)
{
	ui->txtFind->setText(term);
}

void ItemList::stateChange( ContainerState state )
{
	this->state = state;
	ui->add->setVisible(state & (UnsignedContainer | UnsignedSavedContainer | UnencryptedContainer));

	for(auto item: items)
		item->stateChange(state);
}
