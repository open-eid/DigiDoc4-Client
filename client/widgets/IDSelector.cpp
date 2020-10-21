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

#include "IDSelector.h"

#include "DropdownButton.h"
#include "SslCertificate.h"
#include "TokenData.h"

IDSelector::IDSelector(QWidget *parent)
	: QWidget(parent)
	, selector(new DropdownButton(QStringLiteral(":/images/arrow_down.svg"), QStringLiteral(":/images/arrow_down_selected.svg"), this))
{
	selector->hide();
	selector->resize(12, 6);
	selector->move(9, 32);
}

void IDSelector::setList(const QString &selectedCard, const QList<TokenData> &cache, Filter filter)
{
	list.clear();
	for(const TokenData &token: cache)
	{
		if(token.card() == selectedCard)
			continue;
		if(std::any_of(list.cbegin(), list.cend(), [token](const TokenData &item) { return token.card() == item.card(); }))
			continue;
		SslCertificate cert(token.cert());
		if(filter == Signing && !cert.keyUsage().contains(SslCertificate::NonRepudiation))
			continue;
		if(filter == Decrypting && cert.keyUsage().contains(SslCertificate::NonRepudiation))
			continue;
		if(filter == MyEID &&
			!(cert.type() & SslCertificate::EstEidType || cert.type() & SslCertificate::DigiIDType || cert.type() & SslCertificate::TempelType))
			continue;
		list.append(token);
	}
	selector->setHidden(list.isEmpty());
}
