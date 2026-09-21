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

#pragma once

#include <QWidget>

#include "common_enums.h"

namespace libcdoc { struct Lock; }
namespace Ui { class ContainerPage; }

class CryptoDoc;
class DigiDoc;
class MainAction;
class SignatureItem;
class SslCertificate;
class TokenData;
struct WarningText;

class ContainerPage final : public QWidget
{
	Q_OBJECT

public:
	explicit ContainerPage( QWidget *parent = nullptr );
	~ContainerPage() final;

	void setHeader(const QString &file);
	void togglePrinting(bool enable);
	void transition(CryptoDoc *container);
	void transition(DigiDoc* container);

Q_SIGNALS:
	void action(int code);
	void addFiles(const QStringList &files);
	void warning(const WarningText &warningText);

private:
	void changeEvent(QEvent* event) final;
	void clear(int code);
	void decrypt(CryptoDoc *container, const libcdoc::Lock &lock, const QByteArray &secret, const TokenData &token);
	template<class C>
	bool deleteConfirm(C *c, int index);
	void elideFileName();
	void encrypt(CryptoDoc *container);
	bool eventFilter(QObject *o, QEvent *e) final;
	bool isPasswordEncryption() const;
	bool isEncryptEnabled(CryptoDoc *container) const;
	void updatePanes(ria::qdigidoc4::ContainerState state, CryptoDoc *crypto_container);
	void translateLabels();

	Ui::ContainerPage *ui;
	MainAction *mainAction {};
	QString fileName;

	const char *cancelText = QT_TR_NOOP("Cancel");
	const char *convertText = QT_TR_NOOP("Encrypt");
	bool isSupported = false;
};
