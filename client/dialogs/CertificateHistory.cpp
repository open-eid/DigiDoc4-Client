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
#include "common/SslCertificate.h"
#include "dialogs/CertificateDetails.h"

#include <QDebug>
#include <QFile>
#include <QMessageBox>
#include <QStandardPaths>
#include <QtCore/QXmlStreamReader>
#include <QtCore/QXmlStreamWriter>



CertificateHistory::CertificateHistory(QWidget *parent )
:	QDialog( parent )
,	ui(new Ui::CertificateHistory)
{
	ui->setupUi( this );
	QFont condensed = Styles::font(Styles::Condensed, 12);
	ui->close->setFont(condensed);
	ui->select->setFont(condensed);
	ui->remove->setFont(condensed);

	QStringList horzHeaders;
	horzHeaders << tr("Owner") << tr("Type") << tr("Issuer") << tr("Expiry date");
	ui->view->setHeaderLabels(horzHeaders);

	connect(ui->close, &QPushButton::clicked, this, &CertificateHistory::reject);
	connect(ui->select, &QPushButton::clicked, this, &CertificateHistory::select);
	connect(ui->remove, &QPushButton::clicked, this, &CertificateHistory::remove);

	QFile f( path() );
	if( !f.open( QIODevice::ReadOnly ) )
		return;

	QXmlStreamReader xml( &f );
	if( !xml.readNextStartElement() || xml.name() != "History" )
		return;

	while( xml.readNextStartElement() )
	{
		if( xml.name() == "item" )
		{
			QTreeWidgetItem *i = new QTreeWidgetItem( ui->view );
			i->setText( 0, xml.attributes().value( "CN" ).toString() );
			i->setText( 1, xml.attributes().value( "type" ).toString() );
			i->setText( 2, xml.attributes().value( "issuer" ).toString() );
			i->setText( 3, xml.attributes().value( "expireDate" ).toString() );
			ui->view->addTopLevelItem( i );
		}
		xml.skipCurrentElement();
	}

	for(int i = 0; i < 4; i++)
		ui->view->resizeColumnToContents(i);

#ifdef Q_OS_WIN32
	setStyleSheet("QWidget#CertificateHistory{ border-radius: 2px; background-color: #FFFFFF; border: solid #D9D9D8; border-width: 1px 1px 1px 1px;}");
#endif
}

CertificateHistory::~CertificateHistory()
{
	delete ui;
}

QString CertificateHistory::path() const
{
#ifdef Q_OS_WIN
	QSettings s( QSettings::IniFormat, QSettings::UserScope, qApp->organizationName(), "qdigidoccrypto" );
	QFileInfo f( s.fileName() );
	return f.absolutePath() + "/" + f.baseName() + "/certhistory.xml";
#else
	return QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/certhistory.xml";
#endif
}


void CertificateHistory::select()
{
	auto selectedCerts = ui->view->selectedItems();

	for(QTreeWidgetItem* item : selectedCerts)
	{
		qDebug() << item->data(0, Qt::DisplayRole);
	}
}

void CertificateHistory::remove()
{
	qDebug() << "NOT IMPLEMENTED REMOVE";
}
