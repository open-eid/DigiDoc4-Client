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

#include "crypto/CryptoDoc.h"
#include "widgets/Item.h"

#include "SslCertificate.h"

#include <memory>

namespace Ui {
class AddressItem;
}

class QFontMetrics;
class QResizeEvent;

class AddressItem : public Item
{
	Q_OBJECT

public:
	enum ShowToolButton
	{
		None,
		Remove,
		Add,
		Added
	};

	explicit AddressItem(CKey k, QWidget *parent = nullptr, bool showIcon = false);
	~AddressItem() final;

	void disable(bool disable);
	const CKey& getKey() const;
	void idChanged(const QString& cardCode, const QString& mobileCode, const QByteArray& serialNumber) override;
	QWidget* initTabOrder(QWidget *item) override;
	void showButton(ShowToolButton show);
	void stateChange(ria::qdigidoc4::ContainerState state) override;
	void update(const QString& name, const QString& code, SslCertificate::CertType type, const QString& strDate, ShowToolButton show);

protected:
	void changeEvent(QEvent* event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;

private:
	void changeNameHeight();
	void recalculate();
	void setName();
	void setIdType();

	Ui::AddressItem *ui;

	QString code;
	bool enlarged = false;
	CKey key;
	QString name;
	std::unique_ptr<QFontMetrics> nameMetrics;
	int nameWidth = 0;
	int reservedWidth = false;
	SslCertificate::CertType m_type;
	QString expireDateText;
	bool yourself = false;
};
