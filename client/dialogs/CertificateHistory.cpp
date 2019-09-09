/*
 * QDigiDocCrypto
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

#include "CertificateHistory.h"
#include "ui_CertificateHistory.h"

#include "Styles.h"
#include "effects/Overlay.h"


bool HistoryCertData::operator==(const HistoryCertData& other)
{
	return	this->CN == other.CN &&
			this->type == other.type &&
			this->issuer == other.issuer &&
			this->expireDate == other.expireDate;
}

QString HistoryCertData::typeName() const
{
	switch (QString(type).toInt())
	{
	case CertificateHistory::DigiID:
		return CertificateHistory::tr("Digi-ID");
	case CertificateHistory::TEMPEL:
		return CertificateHistory::tr("Certificate for Encryption");
	case CertificateHistory::IDCard:
		return CertificateHistory::tr("ID-card");
	default:
		return CertificateHistory::tr("Other");
	}
}

CertificateHistory::CertificateHistory(QList<HistoryCertData> &_historyCertData, QWidget *parent)
:	QDialog( parent )
,	ui(new Ui::CertificateHistory)
,	historyCertData(_historyCertData)
{
	ui->setupUi(this);
	setWindowFlags( Qt::Dialog | Qt::CustomizeWindowHint );
	setWindowModality( Qt::ApplicationModal );

	QFont condensed = Styles::font(Styles::Condensed, 12);
	ui->close->setFont(condensed);
	ui->select->setFont(condensed);
	ui->remove->setFont(condensed);

	connect(ui->close, &QPushButton::clicked, this, &CertificateHistory::reject);
	connect(ui->select, &QPushButton::clicked, this, [&]{
		emit addSelectedCerts(selectedItems());
	});
	connect(ui->select, &QPushButton::clicked, this, &CertificateHistory::reject);
	connect(ui->remove, &QPushButton::clicked, this, [&]{
		emit removeSelectedCerts(selectedItems());
		fillView();
	});
	connect(ui->view, &QTreeWidget::itemActivated, ui->select, &QPushButton::clicked);

	fillView();
	ui->view->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
	ui->view->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	adjustSize();
}


CertificateHistory::~CertificateHistory()
{
	delete ui;
}

void CertificateHistory::fillView()
{
	ui->view->clear();
	for(const HistoryCertData& certData : historyCertData)
	{
		QTreeWidgetItem *i = new QTreeWidgetItem( ui->view );
		i->setText(0, certData.CN);
		i->setData(1, Qt::UserRole, certData.type);
		i->setText(1, certData.typeName());
		i->setText(2, certData.issuer);
		i->setText(3, certData.expireDate);
		ui->view->addTopLevelItem(i);
	}
}

QList<HistoryCertData> CertificateHistory::selectedItems() const
{
	QList<HistoryCertData> selectedCertData;
	for(QTreeWidgetItem* selectedRow : ui->view->selectedItems())
	{
		selectedCertData.append({
			selectedRow->data(0, Qt::DisplayRole).toString(),
			selectedRow->data(1, Qt::UserRole).toString(),
			selectedRow->data(2, Qt::DisplayRole).toString(),
			selectedRow->data(3, Qt::DisplayRole).toString(),
		});
	}
	return selectedCertData;
}

int CertificateHistory::exec()
{
	Overlay overlay(parentWidget());
	overlay.show();
	auto rc = QDialog::exec();
	overlay.close();

	return rc;
}
