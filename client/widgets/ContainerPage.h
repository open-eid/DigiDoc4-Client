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

#include <memory>

namespace Ui {
class ContainerPage;
}

class CKey;
class CryptoDoc;
class DigiDoc;
class SignatureItem;
class SslCertificate;
class WarningText;

class ContainerPage final : public QWidget
{
	Q_OBJECT

public:
	explicit ContainerPage( QWidget *parent = nullptr );
	~ContainerPage() final;

	void cardChanged(const SslCertificate &cert, bool isBlocked = false);
	void cardChanged(const QString &idCode);
	void clear();
	void setHeader(const QString &file);
	void transition(CryptoDoc *container, bool canDecrypt);
	void transition(DigiDoc* container);
	void update(bool canDecrypt, CryptoDoc *container = nullptr);

signals:
	void action(int code, const QString &info1 = {}, const QString &info2 = {});
	void addFiles(const QStringList &files);
	void details(const QString &id);
	void fileRemoved(int row);
	void keysSelected(const QList<CKey> &keys);
	void moved(const QString &to);
	void removed(int row);
	void warning(const WarningText &warningText);

public slots:
	void clearPopups();
	void togglePrinting(bool enable);

protected:
	void changeEvent(QEvent* event) override;

private:
	void addError(const SignatureItem* item, QMap<ria::qdigidoc4::WarningType, int> &errors);
	void addressSearch();
	bool checkAction(int code, const QString& selectedCard, const QString& selectedMobile);
	void elideFileName(bool force = false);
	bool eventFilter(QObject *o, QEvent *e) override;
	void forward(int code);
	void hideMainAction();
	void hideRightPane();
	void initContainer( const QString &file, const QString &suffix );
	void showMainAction(const QList<ria::qdigidoc4::Actions> &actions);
	void showRightPane(ria::qdigidoc4::ItemType itemType, const QString &header);
	void showSigningButton();
	void updateDecryptionButton();
	void updatePanes(ria::qdigidoc4::ContainerState state);
	void translateLabels();

	Ui::ContainerPage *ui;
	std::unique_ptr<MainAction> mainAction;
	QString cardInReader;
	int containerFileWidth;
	QString fileName;
	QString mobileCode;

	const char *cancelText = "CANCEL";
	const char *convertText = "ENCRYPT";
	bool isSupported = false;
	bool isSeal = false;
	bool isExpired = false;
	bool isBlocked = false;
};
