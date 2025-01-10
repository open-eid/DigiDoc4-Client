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

#include "Application.h"
#include "Styles.h"
#include "SslCertificate.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QSettings>
#include <QtCore/QStandardPaths>
#include <QtCore/QXmlStreamReader>
#include <QtCore/QXmlStreamWriter>

QString HistoryCertData::toType(const SslCertificate &cert)
{
	auto certType = cert.type();
	if(certType & SslCertificate::DigiIDType) return QString::number(CertificateHistory::DigiID);
	if(certType & SslCertificate::TempelType) return QString::number(CertificateHistory::TEMPEL);
	if(certType & SslCertificate::EstEidType) return QString::number(CertificateHistory::IDCard);
	return QString::number(CertificateHistory::Other);
}

QString HistoryCertData::typeName() const
{
	switch (type.toInt())
	{
	case CertificateHistory::DigiID: return CertificateHistory::tr("Digi-ID");
	case CertificateHistory::TEMPEL: return CertificateHistory::tr("Certificate for Encryption");
	case CertificateHistory::IDCard: return CertificateHistory::tr("ID-card");
	default: return CertificateHistory::tr("Other");
	}
}

HistoryList::HistoryList()
{
#ifdef Q_OS_WIN
	QSettings s(QSettings::IniFormat, QSettings::UserScope, Application::organizationName(), QStringLiteral("qdigidoccrypto"));
	QFileInfo info(s.fileName());
	path = info.absolutePath() + '/' + info.baseName() + QLatin1String("/certhistory.xml");
#else
	path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QLatin1String("/certhistory.xml");
#endif

	QFile f(path);
	if(!f.open(QIODevice::ReadOnly))
		return;

	QXmlStreamReader xml(&f);
	if(!xml.readNextStartElement() || xml.name() != QLatin1String("History"))
		return;

	while(xml.readNextStartElement())
	{
		if(xml.name() == QLatin1String("item"))
		{
			append({
				xml.attributes().value(QLatin1String("CN")).toString(),
				xml.attributes().value(QLatin1String("type")).toString(),
				xml.attributes().value(QLatin1String("issuer")).toString(),
				xml.attributes().value(QLatin1String("expireDate")).toString()
			});
		}
		xml.skipCurrentElement();
	}
}

void HistoryList::addAndSave(const QList<SslCertificate> &data)
{
	if(data.isEmpty())
		return;
	bool changed = false;
	for(const auto &cert: data)
	{
		if(cert.isNull())
			continue;
		HistoryCertData certData{
			cert.subjectInfo("CN"),
			HistoryCertData::toType(cert),
			cert.issuerInfo("CN"),
			cert.expiryDate().toLocalTime().toString(QStringLiteral("dd.MM.yyyy")),
		};
		if(contains(certData))
			continue;
		append(std::move(certData));
		changed = true;
	}
	if(changed)
		save();
}

void HistoryList::removeAndSave(const QList<HistoryCertData> &data)
{
	if(data.isEmpty())
		return;
	bool changed = false;
	for(const auto &item: data)
		changed = std::max(changed, removeOne(item));
	if(changed)
		save();
}

void HistoryList::save()
{
	QString p = path;
	QDir().mkpath(QFileInfo(p).absolutePath());
	QFile f(p);
	if(!f.open(QIODevice::WriteOnly|QIODevice::Truncate))
		return;

	QXmlStreamWriter xml(&f);
	xml.setAutoFormatting(true);
	xml.writeStartDocument();
	xml.writeStartElement(QStringLiteral("History"));
	for(const HistoryCertData& certData : *this)
	{
		xml.writeStartElement(QStringLiteral("item"));
		xml.writeAttribute(QStringLiteral("CN"), certData.CN);
		xml.writeAttribute(QStringLiteral("type"), certData.type);
		xml.writeAttribute(QStringLiteral("issuer"), certData.issuer);
		xml.writeAttribute(QStringLiteral("expireDate"), certData.expireDate);
		xml.writeEndElement();
	}
	xml.writeEndDocument();
}



CertificateHistory::CertificateHistory(HistoryList &_historyCertData, QWidget *parent)
:	QDialog( parent )
,	ui(new Ui::CertificateHistory)
,	historyCertData(_historyCertData)
{
	ui->setupUi(this);
	setWindowFlags( Qt::Dialog | Qt::CustomizeWindowHint );
	setAttribute(Qt::WA_DeleteOnClose);
	setMinimumSize(parent->frameSize());
	move(parent->frameGeometry().center() - frameGeometry().center());

	QFont condensed = Styles::font(Styles::Condensed, 12);
	QFont regular = Styles::font(Styles::Regular, 14);
	ui->view->header()->setFont(regular);
	ui->view->setFont(regular);
	ui->close->setFont(condensed);
	ui->select->setFont(condensed);
	ui->remove->setFont(condensed);

	connect(ui->view->selectionModel(), &QItemSelectionModel::selectionChanged, ui->remove, [this] {
		ui->select->setDisabled(ui->view->selectedItems().isEmpty());
		ui->remove->setDisabled(ui->view->selectedItems().isEmpty());
	});
	connect(ui->close, &QPushButton::clicked, this, &CertificateHistory::reject);
	connect(ui->select, &QPushButton::clicked, this, [this]{
		emit addSelectedCerts(selectedItems());
	});
	connect(ui->select, &QPushButton::clicked, this, &CertificateHistory::reject);
	connect(ui->remove, &QPushButton::clicked, this, [this]{
		historyCertData.removeAndSave(selectedItems());
		fillView();
	});
	connect(ui->view, &QTreeWidget::itemActivated, ui->select, &QPushButton::clicked);

	fillView();
	ui->view->header()->setMinimumSectionSize(
		ui->view->fontMetrics().boundingRect(ui->view->headerItem()->text(3)).width() + 25);
	ui->view->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
	ui->view->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	ui->view->sortItems(0, Qt::AscendingOrder);
	adjustSize();
}

CertificateHistory::~CertificateHistory()
{
	delete ui;
}

void CertificateHistory::fillView()
{
	ui->view->clear();
	ui->view->header()->setSortIndicatorShown(!historyCertData.isEmpty());
	for(const HistoryCertData& certData : historyCertData)
	{
		auto *i = new QTreeWidgetItem(ui->view);
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
