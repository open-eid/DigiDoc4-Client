// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QWidget>

#include "QSmartCard.h"

namespace Ui {
class MainWindow;
}

class CryptoDoc;
class DigiDoc;

class MainWindow final : public QWidget
{
	Q_OBJECT

public:
	enum Pages : unsigned char {
		SignIntro,
		SignDetails,
		CryptoIntro,
		CryptoDetails,
		MyEid
	};
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow() noexcept final;

	void openFiles(QStringList files, bool addFile = false, bool forceCreate = false);
	void selectPage(Pages page);
	void showSettings(int page);

protected:
	void changeEvent(QEvent* event) final;
	void closeEvent(QCloseEvent *event) final;
	void dragEnterEvent( QDragEnterEvent *event ) final;
	void dragLeaveEvent( QDragLeaveEvent *event ) final;
	void dropEvent( QDropEvent *event ) final;
	void showEvent(QShowEvent *event) final;

private:
	void adjustDrops();
	void convertToCDoc();
	void navigateToPage(Pages page, const QStringList &files = QStringList(), bool create = true);
	void onCryptoAction(int action);
	void onSignAction(int action);
	void openContainer(const QString &filter);
	void resetDigiDoc(std::unique_ptr<DigiDoc> &&doc);
	void sign();
	void updateMyEid(const QSmartCardData &data);
	bool wrap(const QString& wrappedFile, bool pdf);

	static QStringList dropEventFiles(QDropEvent *event);

	std::unique_ptr<CryptoDoc> cryptoDoc;
	std::unique_ptr<DigiDoc> digiDoc;
	std::unique_ptr<Ui::MainWindow> ui;
};
