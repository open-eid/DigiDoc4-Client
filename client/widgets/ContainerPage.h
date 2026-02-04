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

#include "common_enums.h"
#include "widgets/MainAction.h"
#include "CryptoDoc.h"

#include <memory>

namespace Ui {
class ContainerPage;
}

class CryptoDoc;
class DigiDoc;
class QSslCertificate;
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

	void cardChanged(const SslCertificate &cert, bool isBlocked = false);
	void tokenChanged(const TokenData &token);
	void clearPopups();
	void setHeader(const QString &file);
	void togglePrinting(bool enable);
	void transition(CryptoDoc *container, const QSslCertificate &cert);
	void transition(DigiDoc* container);

Q_SIGNALS:
	void action(int code, const QString &idCode = {}, const QString &info2 = {});
	void addFiles(const QStringList &files);
	void certChanged(const SslCertificate &cert);
	void removed(int row);
	void warning(const WarningText &warningText);

	void decryptReq(const libcdoc::Lock *key);

private:
	void changeEvent(QEvent* event) final;
	void clear(int code);
	template<class C>
	void deleteConfirm(C *c, int index);
	void elideFileName();
	bool eventFilter(QObject *o, QEvent *e) final;
	void showMainAction(const QList<ria::qdigidoc4::Actions> &actions);
	void showSigningButton();
	void handleAction(int type);
	void updateDecryptionButton();
	void updatePanes(ria::qdigidoc4::ContainerState state, CryptoDoc *crypto_container);
	void translateLabels();

	Ui::ContainerPage *ui;
	std::unique_ptr<MainAction> mainAction;
	QString idCode;
	QString fileName;

	const char *cancelText = QT_TR_NOOP("Cancel");
	const char *convertText = QT_TR_NOOP("Encrypt");
	bool isSupported = false;
	bool isSeal = false;
	bool isExpired = false;
	bool isBlocked = false;
};
