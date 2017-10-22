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

#include "DigiDoc.h"
#include "widgets/Item.h"

#include <memory>

namespace Ui {
class SignatureItem;
}

class QFontMetrics;

class SignatureItem : public Item
{
	Q_OBJECT

public:
	explicit SignatureItem(const DigiDocSignature &s, ria::qdigidoc4::ContainerState state, QWidget *parent = nullptr);
	~SignatureItem();

	void idChanged(const QString& cardCode, const QString& mobileCode) override;
	bool isSelfSigned(const QString& cardCode, const QString& mobileCode) const;

protected:
	void mouseReleaseEvent(QMouseEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;

private:
	void changeNameHeight();
	void recalculate();
	QString red(const QString &text);
	void setIcon(const QString &icon, int width = 17, int height = 20);

	Ui::SignatureItem *ui;
	DigiDocSignature signature;

	bool enlarged;
	bool invalid;
	int nameWidth;
	int reservedWidth;
	QString name;
	QString serial;
	QString status;
	QString statusHtml;
	std::unique_ptr<QFontMetrics> nameMetrics;
};
