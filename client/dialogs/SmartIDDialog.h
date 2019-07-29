#pragma once

#include <QDialog>

namespace Ui { class SmartIDDialog; }

class SmartIDDialog : public QDialog
{
	Q_OBJECT

public:
	explicit SmartIDDialog(QWidget *parent = nullptr);
	~SmartIDDialog() final;

	QString country() const;
	QString idCode() const;

private:
	Ui::SmartIDDialog *ui;
};
