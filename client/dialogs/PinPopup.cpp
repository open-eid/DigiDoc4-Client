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

#include "PinPopup.h"
#include "ui_PinPopup.h"
#include "SslCertificate.h"
#include "effects/Overlay.h"

#include <QtCore/QTimeLine>
#include <QtGui/QRegularExpressionValidator>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QSvgWidget>

PinPopup::PinPopup(PinFlags flags, const SslCertificate &c, TokenFlags count, QWidget *parent)
	: PinPopup(flags, c.toString(c.showCN() ? QStringLiteral("CN, serialNumber") : QStringLiteral("GN SN, serialNumber")), count, parent)
{
	if(c.type() & SslCertificate::TempelType)
	{
		regexp->setRegularExpression(QRegularExpression(QStringLiteral("^.{4,}$")));
		ui->pin->setMaxLength(32767);
	}
}

PinPopup::PinPopup(PinFlags flags, const QString &title, TokenFlags count, QWidget *parent, QString text)
	: QDialog(parent)
	, ui(new Ui::PinPopup)
{
	ui->setupUi(this);
	setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint);
#ifdef Q_OS_WIN
	ui->buttonLayout->setDirection(QBoxLayout::RightToLeft);
#endif
	for(QLineEdit *w: findChildren<QLineEdit*>())
		w->setAttribute(Qt::WA_MacShowFocusRect, false);
	ui->pin->setValidator(regexp = new QRegularExpressionValidator(ui->pin));
	new Overlay(this);

	connect( ui->ok, &QPushButton::clicked, this, &PinPopup::accept );
	connect( ui->cancel, &QPushButton::clicked, this, &PinPopup::reject );
	connect( this, &PinPopup::finished, this, &PinPopup::close );
	connect(ui->pin, &QLineEdit::returnPressed, ui->ok, &QPushButton::click);

	if(!text.isEmpty())
		ui->labelPin->hide();
	else if( flags & Pin2Type )
	{
		text = tr("Selected action requires sign certificate.");
		ui->labelPin->setText(flags & PinpadFlag ?
			tr("For using sign certificate enter PIN2 at the reader") :
			tr("For using sign certificate enter PIN2"));
		setPinLen(5);
	}
	else
	{
		text = tr("Selected action requires authentication certificate.");
		ui->labelPin->setText(flags & PinpadFlag ?
			tr("For using authentication certificate enter PIN1 at the reader") :
			tr("For using authentication certificate enter PIN1"));
		setPinLen(4);
	}
	ui->label->setText(title);
	ui->text->setText(text);
	if(count & PinFinalTry)
		ui->errorPin->setText(tr("PIN will be locked next failed attempt"));
	else if(count & PinCountLow)
		ui->errorPin->setText(tr("PIN has been entered incorrectly at least once"));
	else
		ui->errorPin->hide();

	if(flags & PinpadChangeFlag)
	{
		ui->pin->hide();
		ui->ok->hide();
		ui->cancel->hide();
		auto *movie = new QSvgWidget(QStringLiteral(":/images/wait.svg"), this);
		movie->setFixedSize(ui->pin->size().height(), ui->pin->size().height());
		movie->show();
		ui->layoutContent->addWidget(movie, 0, Qt::AlignCenter);
	}
	if( flags & PinpadFlag )
	{
		ui->pin->hide();
		ui->ok->hide();
		ui->cancel->hide();
		auto *progress = new QProgressBar(this);
		progress->setRange( 0, 30 );
		progress->setValue( progress->maximum() );
		progress->setTextVisible( false );
		progress->resize( 200, 30 );
		progress->move( 153, 122 );
		ui->layoutContent->addWidget(progress);
		auto *statusTimer = new QTimeLine(progress->maximum() * 1000, this);
		statusTimer->setEasingCurve(QEasingCurve::Linear);
		statusTimer->setFrameRange( progress->maximum(), progress->minimum() );
		connect( statusTimer, &QTimeLine::frameChanged, progress, &QProgressBar::setValue );
		connect( this, &PinPopup::startTimer, statusTimer, &QTimeLine::start );
	}
	else
	{
		ui->pin->setFocus();
		ui->pin->setMaxLength( 12 );
		connect(ui->pin, &QLineEdit::textEdited, this, [&](const QString &text) {
			ui->ok->setEnabled(regexp->regularExpression().match(text).hasMatch());
		});
		ui->text->setBuddy( ui->pin );
		ui->ok->setDisabled(true);
	}
}

PinPopup::~PinPopup()
{
	delete ui;
}

void PinPopup::setPinLen(unsigned long minLen, unsigned long maxLen)
{
	QString charPattern = regexp->regularExpression().pattern().startsWith(QLatin1String("^.")) ? QStringLiteral(".") : QStringLiteral("\\d");
	regexp->setRegularExpression(QRegularExpression(QStringLiteral("^%1{%2,%3}$").arg(charPattern).arg(minLen).arg(maxLen)));
}

QString PinPopup::pin() const { return ui->pin->text(); }
