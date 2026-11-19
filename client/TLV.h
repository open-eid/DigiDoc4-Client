// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QtCore/QByteArrayView>

#include <iterator>

struct TLV
{
	quint32 tag {};
	quint32 length {};
	QByteArrayView data;

	constexpr TLV(QByteArrayView _data)
	{
		if (_data.isEmpty())
			return;

		auto begin = _data.cbegin();
		tag = quint8(*begin++);
		if((tag & 0x1F) == 0x1F) { // Multi-byte tag
			constexpr quint8 MAX_TAG_BYTES = sizeof(tag);
			quint8 tag_bytes = 1;
			do {
				if(tag_bytes >= MAX_TAG_BYTES || begin == _data.cend())
					return;
				tag = (tag << 8) | quint8(*begin++);
				++tag_bytes;
			} while ((tag & 0x80) != 0x00);
		}

		if (begin == _data.cend())
			return;

		length = quint8(*begin++);
		if (length & 0x80) { // Extended length encoding
			constexpr quint8 MAX_LEN_BYTES = sizeof(length);
			auto len_bytes = quint8(length & 0x7F);
			if (len_bytes == 0 || len_bytes > MAX_LEN_BYTES
				|| std::distance(begin, _data.cend()) < len_bytes) {
				return;
			}

			length = 0;
			for (quint8 i = 0; i < len_bytes; ++i) {
				length = (length << 8) | quint8(*begin++);
			}
		}

		if (std::distance(begin, _data.cend()) >= length) {
			data = QByteArrayView(begin, _data.cend());
		}
	}

	constexpr TLV next() const { return *this ? TLV(data.sliced(length)) : TLV({}); }
	constexpr TLV find(quint32 find) const
	{
		TLV tlv = *this;
		for (; tlv && tlv.tag != find; ++tlv) {}
		return tlv;
	}
	constexpr QByteArrayView value() const { return *this ? data.first(length) : QByteArrayView(); }

	constexpr TLV operator[](quint32 find) const { return TLV(value()).find(find); }
	constexpr TLV& operator++() { return *this = next(); }
	constexpr operator bool() const { return !data.isEmpty(); }
};
