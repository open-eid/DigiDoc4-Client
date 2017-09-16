#pragma once

#include "widgets/StyledWidget.h"

namespace Ui {
class SignatureItem;
}

class SignatureItem : public StyledWidget
{
	Q_OBJECT

public:
	explicit SignatureItem(ria::qdigidoc4::ContainerState state, QWidget *parent = 0);
	~SignatureItem();

	void stateChange(ria::qdigidoc4::ContainerState state) override;
	
private:
	Ui::SignatureItem *ui;
};

