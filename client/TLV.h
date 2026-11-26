// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QByteArrayView>

#include <iterator>
#include <type_traits>
#include <utility>

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
	constexpr operator bool() const { return !data.isNull(); }
};

// BER encoder counterpart of TLV, writes single-byte tags only
template<typename T>
struct Ber final
{
	quint8 tag;
	T value;

	static constexpr int lengthSize(qsizetype size)
	{
		int length = 1;
		if(size >= 0x80) for(; size > 0; size >>= 8)
			++length;
		return length;
	}

	constexpr qsizetype payloadSize() const
	{
		if constexpr(std::is_same_v<T, int>)
		{
			qsizetype size = 1;
			for(int n = value; n < -128 || n > 127; n >>= 8)
				++size;
			return size;
		}
		else if constexpr(std::is_convertible_v<const T&, QByteArrayView>)
			return QByteArrayView(value).size();
		else
			return value.encodedSize();
	}

	constexpr qsizetype encodedSize() const
	{
		qsizetype size = payloadSize();
		return 1 + lengthSize(size) + size;
	}

	operator QByteArray() const;
};

template<typename T>
inline QByteArray operator+(QByteArray lhs, const Ber<T>& rhs)
{
	// Tag
	lhs.append(char(rhs.tag));
	// Length
	auto size = rhs.payloadSize();
	if(int length = Ber<T>::lengthSize(size); length == 1)
		lhs.append(char(size));
	else
	{
		lhs.append(char(0x80 | (length - 1)));
		for(int shift = (length - 2) * 8; shift >= 0; shift -= 8)
			lhs.append(char((size >> shift) & 0xFF));
	}
	// Value
	if constexpr(std::is_convertible_v<const T&, QByteArrayView>)
		lhs.append(QByteArrayView(rhs.value));
	else if constexpr(std::is_same_v<T, int>)
	{
		for(int shift = (size - 1) * 8; shift >= 0; shift -= 8)
			lhs.append(char((quint32(rhs.value) >> shift) & 0xFF));
	}
	else
		return std::move(lhs) + rhs.value;
	return lhs;
}

template<typename T>
Ber<T>::operator QByteArray() const
{
	QByteArray result;
	result.reserve(encodedSize());
	return std::move(result) + *this;
}
