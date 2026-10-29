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

#include "AddressItem.h"
#include "ui_AddressItem.h"

#include "CryptoDoc.h"
#include "Settings.h"
#include "SslCertificate.h"
#include "dialogs/KeyDialog.h"

#include <QtCore/QDateTime>
#include <QtWidgets/QBoxLayout>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QStyle>

using namespace ria::qdigidoc4;

struct AddressItem::Private: public Ui::AddressItem
{
	Private(Type type) : type(type) {}

	const Type type;
	QString code;
	// Keys of one person, sharing the card; the first one names it
	std::vector<CKey> keys;
	// One line per key: a checkbox tile on an Add card, a badge on the others
	struct Tile {
		QWidget *tile;
		QCheckBox *check;
		// QCheckBox cannot wrap "Mobile-ID / Smart-ID" in a narrow tile; a label can
		QLabel *text;
	};
	std::vector<Tile> tiles;
	std::vector<QLabel*> badges;
	QGridLayout *checkLayout {};
	QString label;
	ContainerState state = UnencryptedContainer;
};

AddressItem::AddressItem(const CKey &key, Type type, QWidget *parent)
	: Item(parent)
	, ui(new Private(type))
{
	ui->setupUi(this);
	setProperty("card", type != Icon);
	ui->icon->load(QStringLiteral(":/images/icon_Minu_eID_hover.svg"));
	ui->name->setAttribute(Qt::WA_TransparentForMouseEvents, true);
	if(type == Add) {
		ui->checkLayout = new QGridLayout;
		ui->checkLayout->setSpacing(8);
		ui->AddressItemLayout->addLayout(ui->checkLayout, 1, 0, 1, -1);
	}

	bool unsupported = !setRecipient(key);
	if(!key.rcpt_cert.isNull())
		ui->decrypt->hide();

	if(!unsupported)
		setCursor(Qt::PointingHandCursor);

	connect(ui->add, &QToolButton::clicked, this, [this]{ emit add(this);});
	connect(ui->remove, &QToolButton::clicked, this, [this]{ emit remove(this);});
	connect(ui->decrypt, &QToolButton::clicked, this, &AddressItem::decrypt);

	setName();
	addKey(key);
	setName();
	ui->add->setVisible(type == Add);
	ui->remove->setVisible(type != Add);
}

AddressItem::~AddressItem()
{
	delete ui;
}

void AddressItem::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::LanguageChange)
	{
		ui->retranslateUi(this);
		setName();
		for(size_t i = 0; i < ui->keys.size(); ++i)
			setKeyText(i);
	}
	// retranslateUi() resets the text to the .ui's "Add"
	if (event->type() == QEvent::LanguageChange || event->type() == QEvent::EnabledChange)
		ui->add->setText(isEnabled() ? tr("Add") : tr("Added"));
	QWidget::changeEvent(event);
}

bool AddressItem::eventFilter(QObject *watched, QEvent *event)
{
	// A click anywhere on a tile toggles it, and does not open the key details
	if(auto *tile = qobject_cast<QWidget*>(watched); tile && tile->objectName() == QLatin1String("tile")) {
		if(event->type() == QEvent::MouseButtonPress)
			return true;
		if(event->type() == QEvent::MouseButtonRelease) {
			if(auto *check = tile->findChild<QCheckBox*>(); check && check->isEnabled())
				check->toggle();
			return true;
		}
	}
	return Item::eventFilter(watched, event);
}

void AddressItem::setAddEnabled()
{
	ui->add->setDisabled(std::ranges::none_of(ui->tiles, [](const auto &t) { return t.check->isChecked(); }));
}

bool AddressItem::setRecipient(const CKey &key)
{
	if (!key.rcpt_cert.isNull()) {
		// Recipient certificate
		ui->code = SslCertificate(key.rcpt_cert).personalCode();
		ui->label = !key.rcpt_cert.subjectInfo("GN").isEmpty() &&
							!key.rcpt_cert.subjectInfo("SN").isEmpty()
						? key.rcpt_cert.subjectInfo("GN").join(' ') + ' ' +
							  key.rcpt_cert.subjectInfo("SN").join(' ')
						: key.rcpt_cert.subjectInfo("CN").join(' ');
	} else if (key.lock.type == libcdoc::Lock::SHARE_SERVER) {
		// Mobile-ID / Smart-ID: only the personal code is known, and libcdoc
		// writes no label for it, so the lock does not pass isValid()
		ui->code = key.personalCode();
		ui->label.clear();
	} else if (key.lock.isValid()) {
		// Known lock type
		ui->code.clear();
		auto map = libcdoc::Lock::parseLabel(key.lock.label);
		if (map.contains("cn")) {
			ui->label = QString::fromStdString(map["cn"]);
		} else if (map.contains("label")) {
			ui->label = QString::fromStdString(map["label"]);
		} else {
			ui->label = QString::fromStdString(key.lock.label);
		}
	} else {
		// No rcpt, lock is invalid = unsupported lock
		ui->code.clear();
		ui->label = tr("Unsupported cryptographic algorithm or recipient type");
		return false;
	}
	return true;
}

void AddressItem::addKey(const CKey &key)
{
	// A card started from a key share has no name until a certificate brings it
	if(ui->label.isEmpty() && !key.rcpt_cert.isNull()) {
		setRecipient(key);
		setName();
	}
	// Mobile-ID / Smart-ID stays last, after the person's certificates
	auto isShare = [](const CKey &k) { return k.lock.type == libcdoc::Lock::SHARE_SERVER; };
	size_t pos = isShare(key) ? ui->keys.size() :
		size_t(std::ranges::find_if(ui->keys, isShare) - ui->keys.cbegin());
	ui->keys.insert(ui->keys.begin() + qsizetype(pos), key);
	if(ui->type == Add) {
		auto *tile = new QWidget(this);
		tile->setObjectName(QStringLiteral("tile"));
		tile->setAttribute(Qt::WA_StyledBackground);
		tile->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		tile->setCursor(Qt::PointingHandCursor);
		tile->installEventFilter(this);
		auto *check = new QCheckBox(tile);
		// The macOS style otherwise lays it out by a visual rect that the label overlaps
		check->setAttribute(Qt::WA_LayoutUsesWidgetRect);
		auto *text = new QLabel(tile);
		text->setWordWrap(true);
		text->setAttribute(Qt::WA_TransparentForMouseEvents, true);
		auto *tileLayout = new QHBoxLayout(tile);
		tileLayout->setContentsMargins(8, 8, 8, 8);
		tileLayout->setSpacing(8);
		tileLayout->addWidget(check);
		tileLayout->addWidget(text, 1);
		ui->tiles.insert(ui->tiles.begin() + qsizetype(pos), {tile, check, text});
		// Equal tiles, at most three to a row as in the design
		constexpr int columns = 3;
		for(const auto &t: ui->tiles)
			ui->checkLayout->removeWidget(t.tile);
		for(int i = 0; i < columns; ++i)
			ui->checkLayout->setColumnStretch(i, i < int(ui->tiles.size()) ? 1 : 0);
		for(size_t i = 0; i < ui->tiles.size(); ++i)
			ui->checkLayout->addWidget(ui->tiles[i].tile, int(i) / columns, int(i) % columns);
		connect(check, &QCheckBox::toggled, this, [this, tile](bool checked) {
			tile->setProperty("checked", checked);
			tile->style()->unpolish(tile);
			tile->style()->polish(tile);
			setAddEnabled();
		});
		check->setChecked(true);
		setKeyText(pos);
		setAddEnabled();
		return;
	}
	auto *badge = new QLabel(this);
	badge->setFocusPolicy(Qt::TabFocus);
	badge->setAttribute(Qt::WA_TransparentForMouseEvents, true);
	ui->badges.push_back(badge);
	// Each key gets its own line under the name; the side widgets span them all
	int rows = int(ui->badges.size()) + 1;
	ui->AddressItemLayout->addWidget(badge, rows - 1, 1, Qt::AlignLeft);
	for(QWidget *w: {(QWidget*)ui->icon, (QWidget*)ui->remove, (QWidget*)ui->add, (QWidget*)ui->decrypt}) {
		int row{}, column{}, rowSpan{}, columnSpan{};
		ui->AddressItemLayout->getItemPosition(ui->AddressItemLayout->indexOf(w), &row, &column, &rowSpan, &columnSpan);
		ui->AddressItemLayout->removeWidget(w);
		ui->AddressItemLayout->addWidget(w, row, column, rows, columnSpan);
	}
	// Badges hold no state, so a key inserted ahead of others just shifts the texts
	for(size_t i = pos; i < ui->keys.size(); ++i)
		setKeyText(i);
	setDecryptVisible();
}

const CKey& AddressItem::getKey() const
{
	return ui->keys.front();
}

const std::vector<CKey>& AddressItem::keys() const
{
	return ui->keys;
}

std::vector<CKey> AddressItem::selectedKeys() const
{
	if(ui->type != Add)
		return ui->keys;
	std::vector<CKey> result;
	for(size_t i = 0; i < ui->keys.size(); ++i) {
		if(ui->tiles[i].check->isChecked())
			result.push_back(ui->keys[i]);
	}
	return result;
}

void AddressItem::initTabOrder(QWidget *item)
{
	QWidget *prev = item;
	auto chain = [&prev](QWidget *next) {
		setTabOrder(prev, next);
		prev = next;
	};
	chain(ui->name);
	for(const auto &t: ui->tiles)
		chain(t.check);
	for(QWidget *badge: ui->badges)
		chain(badge);
	chain(ui->remove);
	chain(lastTabWidget());
}

QWidget* AddressItem::lastTabWidget()
{
	return ui->add;
}

void AddressItem::mouseReleaseEvent(QMouseEvent * /*event*/) {
	(new KeyDialog(ui->keys.front(), this))->open();
}

void AddressItem::setDecryptVisible()
{
	ui->decrypt->setVisible(ui->state == EncryptedContainer &&
		std::ranges::any_of(ui->keys, [](const CKey &key) {
			return key.lock.isPKI() || key.lock.type == libcdoc::Lock::PASSWORD ||
				key.lock.type == libcdoc::Lock::SHARE_SERVER;
		}));
}

QString AddressItem::name() const
{
	if(ui->code.isEmpty() || ui->label.isEmpty())
		return ui->code.isEmpty() ? ui->label : ui->code;
	return QStringLiteral("%1, %2").arg(ui->label, ui->code);
}

void AddressItem::setName()
{
	ui->name->setText(ui->code.isEmpty() || ui->label.isEmpty() ? name().toHtmlEscaped() :
		QStringLiteral("%1, <span style=\"font-weight:normal;\">%2</span>")
			.arg(ui->label.toHtmlEscaped(), ui->code.toHtmlEscaped()));
	if(ui->name->text().isEmpty())
		ui->name->hide();
}

void AddressItem::stateChange(ContainerState state)
{
	ui->state = state;
	if(ui->type == Icon)
		ui->icon->load(state == EncryptedContainer ?
			QStringLiteral(":/images/icon_Krypto_small.svg") : QStringLiteral(":/images/icon_Minu_eID_hover.svg"));
	if(ui->add->isHidden())
		ui->remove->setVisible(state == UnencryptedContainer);
	setDecryptVisible();
}

void AddressItem::setKeyText(size_t i)
{
	const CKey &key = ui->keys[i];
	QString type;
	// Until when the key can open the container: the certificate's own expiry, or
	// the capsule's on the key server. The latter is known only once encrypted.
	QDateTime until;
	SslCertificate cert(key.rcpt_cert);
	if(cert.isNull() && key.lock.isCDoc1()) {
		const auto &bytes = key.lock.getBytes(libcdoc::Lock::CERT);
		cert = SslCertificate(QByteArray::fromRawData((const char *)bytes.data(), qsizetype(bytes.size())), QSsl::Der);
	}
	if(!cert.isNull()) {
		type = typeName(cert);
		until = cert.expiryDate();
		if(!key.rcpt_cert.isNull() && cert.isValid() && Settings::CDOC2_DEFAULT && Settings::CDOC2_USE_KEYSERVER)
			until = {};
	} else if(key.lock.type == libcdoc::Lock::SHARE_SERVER) {
		type = tr("Mobile-ID / Smart-ID");
	} else if(key.lock.isValid()) {
		// Needed to include translation for "ID-CARD"
		void(QT_TR_NOOP("ID-CARD"));
		void(QT_TR_NOOP("pw"));
		auto items = libcdoc::Lock::parseLabel(key.lock.label);
		type = tr(items["type"].data());
		if(const std::string &exp = items["server_exp"]; !exp.empty())
			until = QDateTime::fromSecsSinceEpoch(QString::fromStdString(exp).toLongLong());
	}

	bool expired = until.isValid() && until <= QDateTime::currentDateTimeUtc();
	QString text = type;
	if(until.isValid())
		text = (expired ? tr("%1 (decryption expired %2)") : tr("%1 (decryption until %2)"))
			.arg(type, until.toLocalTime().toString(QStringLiteral("dd.MM.yyyy")));

	if(ui->type == Add) {
		// An expired key cannot be ticked; its date is too long for the tile
		const auto &t = ui->tiles[i];
		t.text->setText(type);
		t.tile->setToolTip(expired ? text : QString());
		if(expired)
			t.check->setChecked(false);
		t.tile->setDisabled(expired);
		return;
	}
	QLabel *badge = ui->badges[i];
	badge->setText(text);
	badge->setProperty("label", expired ? QStringLiteral("error") :
		key.lock.type == libcdoc::Lock::PASSWORD ? QStringLiteral("default") : QStringLiteral("good"));
	badge->setHidden(text.isEmpty());
}

QString AddressItem::typeName(const SslCertificate &cert) const
{
	SslCertificate::CertType type = cert.type();
	if(type & SslCertificate::DigiIDType)
		return tr("digi-ID");
	if(type & SslCertificate::EstEidType)
		return tr("ID-card");
	if(type & SslCertificate::MobileIDType)
		return tr("mobile-ID");
	if(type & SslCertificate::TempelType)
	{
		if(cert.keyUsage().contains(SslCertificate::NonRepudiation))
			return tr("e-Seal");
		if(cert.enhancedKeyUsage().contains(SslCertificate::ClientAuth))
			return tr("Authentication certificate");
	}
	// An unrecognised certificate would otherwise leave its tile unlabelled
	return tr("Certificate for Encryption");
}
