// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "StyledWidget.h"

#include "MainWindow.h"

namespace Ui { class WarningItem; }

struct WarningText {
	enum WarningType : unsigned char {
		NoWarning = 0,

		CertExpiredError,
		CertExpiryWarning,
		UnblockPin1Warning,
		UnblockPin2Warning,
		ActivatePin2Warning,
		ActivatePin1WithPUKWarning,
		ActivatePin2WithPUKWarning,
		LockedCardWarning,

		InvalidSignatureError,
		InvalidTimestampError,
		UnknownSignatureWarning,
		UnknownTimestampWarning,
		UnsupportedAsicSWarning,
		UnsupportedAsicCadesWarning,
		UnsupportedDDocWarning,
		UnsupportedCDocWarning,
		EmptyFileWarning,
	} type = NoWarning;
	int counter = 0;
	std::function<void()> cb;
};


class WarningItem final: public StyledWidget
{
	Q_OBJECT

public:
	WarningItem(WarningText warningText, QWidget *parent = nullptr);
	~WarningItem() final;
	
	int page() const;
	WarningText::WarningType warningType() const;

private:
	Q_DISABLE_COPY(WarningItem)
	void changeEvent(QEvent *event) final;
	void mousePressEvent(QMouseEvent *event) final;
	void lookupWarning();

	Ui::WarningItem *ui;
	WarningText warnText;
	QString url;
	MainWindow::Pages _page = MainWindow::MyEid;
};
