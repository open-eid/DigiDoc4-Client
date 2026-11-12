// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "TLV.h"

#include <QtTest/QTest>

#include <array>
#include <limits>

namespace {

// String literals may contain 0x00 bytes, so the size comes from the array, not strlen
template<std::size_t N>
constexpr TLV parse(const char (&der)[N])
{
	return TLV(QByteArrayView(der, qsizetype(N - 1)));
}

template<std::size_t N>
constexpr TLV parse(const std::array<char, N> &der)
{
	return TLV(QByteArrayView(der.data(), qsizetype(N)));
}

template<std::size_t Size, std::size_t N>
constexpr std::array<char, N - 1 + Size> withValue(const char (&header)[N])
{
	std::array<char, N - 1 + Size> der{};
	for(std::size_t i = 0; i < der.size(); ++i)
		der[i] = i < N - 1 ? header[i] : 'x';
	return der;
}

constexpr bool equals(QByteArrayView lhs, QByteArrayView rhs)
{
	if(lhs.size() != rhs.size())
		return false;
	for(qsizetype i = 0; i < lhs.size(); ++i)
	{
		if(lhs[i] != rhs[i])
			return false;
	}
	return true;
}

template<std::size_t N>
constexpr int count(const char (&der)[N])
{
	int n = 0;
	for(TLV tlv = parse(der); tlv; ++tlv)
		++n;
	return n;
}

// Invalid input
static_assert(!TLV({}));
static_assert(!parse("\x04"));
static_assert(!parse("\x04\x03\x41\x42"));
static_assert(parse("\x04\x03\x41\x42").value().isEmpty());
static_assert(!parse("\x04\x03\x41\x42").next());
static_assert(!parse("\x04\x03\x41\x42")[0x04]);

// Primitive
static_assert(parse("\x04\x02\x41\x42").tag == 0x04);
static_assert(parse("\x04\x02\x41\x42").length == 2);
static_assert(equals(parse("\x04\x02\x41\x42").value(), "AB"));
static_assert(equals(parse("\x02\x01\x05\xFF\xFF").value(), "\x05"));

// Zero length, also as the last element
static_assert(parse("\x04\x00"));
static_assert(parse("\x04\x00").tag == 0x04);
static_assert(parse("\x04\x00").length == 0);
static_assert(parse("\x04\x00").value().isEmpty());
static_assert(!parse("\x04\x00").next());
static_assert(parse("\x04\x81\x00"));
static_assert(parse("\x04\x81\x00").length == 0);

// Multi-byte tags
static_assert(parse("\xDF\x21\x01\x03").tag == 0xDF21);
static_assert(parse("\xBF\x81\x01\x01\x03").tag == 0xBF8101);
static_assert(parse("\xDF\x81\x81\x01\x01\x03").tag == 0xDF818101);
static_assert(equals(parse("\xDF\x81\x81\x01\x01\x03").value(), "\x03"));
static_assert(!parse("\xDF\x81\x81\x81\x01\x01\x03"));
static_assert(!parse("\xBF\x81"));
static_assert(!parse("\xDF\x21"));

// Long-form lengths
constexpr auto longLength1 = withValue<0x80>("\x04\x81\x80");
static_assert(parse(longLength1).length == 0x80);
static_assert(equals(parse(longLength1).value(), QByteArrayView(longLength1.data() + 3, 0x80)));
constexpr auto longLength2 = withValue<0x100>("\x04\x82\x01\x00");
static_assert(parse(longLength2).length == 0x100);
static_assert(equals(parse("\x04\x84\x00\x00\x00\x02\x41\x42").value(), "AB"));
static_assert(equals(parse("\x04\x81\x02\x41\x42").value(), "AB"));
static_assert(!parse("\x30\x80\x00\x00"));
static_assert(!parse("\x04\x85\x00\x00\x00\x00\x02\x41\x42"));
static_assert(!parse("\x04\x82\x01"));
static_assert(!parse("\x04\x82\x01\x00\x41\x42"));
static_assert(!parse("\x04\x84\xFF\xFF\xFF\xFF\x41\x42"));

// LdapSearch drops a parsed message by data.data() - begin + length
constexpr char shortForm[] = "\xBF\x81\x01\x02\x41\x42";
static_assert(parse(shortForm).data.data() - shortForm == 4);
constexpr char longForm[] = "\x04\x82\x00\x02\x41\x42";
static_assert(parse(longForm).data.data() - longForm == 4);

// Siblings
constexpr char siblings[] = "\x02\x01\x05\x04\x02\x41\x42\x01\x01\xFF";
static_assert(count(siblings) == 3);
static_assert(parse(siblings).next().tag == 0x04);
static_assert(equals(parse(siblings).next().value(), "AB"));
static_assert(parse(siblings).next().next().tag == 0x01);
static_assert(!parse(siblings).next().next().next());
static_assert(count("\x04\x00\x02\x01\x05") == 2);
static_assert(equals(parse("\x04\x00\x02\x01\x05").next().value(), "\x05"));
static_assert(count("\x02\x01\x05\x04\x00") == 2);
static_assert(parse("\x02\x01\x05\x04\x00").find(0x04));
static_assert(count("\x02\x01\x05\x04\x05\x41") == 1);
static_assert(!parse("\x02\x01\x05\x04\x05\x41").find(0x04));

// find() only looks forward from the current element
constexpr char counters[] = "\x02\x01\x05\xDF\x21\x01\x03\xDF\x2F\x01\x00";
static_assert(equals(parse(counters).find(0x02).value(), "\x05"));
static_assert(equals(parse(counters).find(0xDF21).value(), "\x03"));
static_assert(parse(counters).find(0xDF2F).value().size() == 1);
static_assert(parse(counters).find(0xDF2F).value()[0] == 0);
static_assert(!parse(counters).find(0xDF22));
static_assert(!parse(counters).next().find(0x02));

// IDEMIA PIN counter response layout: 70 { BF8101 { A0 { 9A, 9B } } }
constexpr char idemia[] = "\x70\x0C\xBF\x81\x01\x08\xA0\x06\x9A\x01\x03\x9B\x01\x02";
static_assert(parse(idemia)[0xBF8101][0xA0].tag == 0xA0);
static_assert(equals(parse(idemia)[0xBF8101][0xA0][0x9B].value(), "\x02"));
static_assert(equals(parse(idemia)[0xBF8101][0xA0][0x9A].value(), "\x03"));
static_assert(!parse(idemia)[0xBF8101][0xA0][0x9C]);
static_assert(!parse(idemia)[0xBF8102]);
static_assert(!parse(idemia)[0xBF8102][0xA0][0x9B]);
// operator[] searches children, not the element itself
static_assert(!parse(idemia)[0x70]);
// An empty counter must not expose the bytes that follow it
static_assert(parse("\xA0\x05\x9B\x00\x9A\x01\x03")[0x9B]);
static_assert(parse("\xA0\x05\x9B\x00\x9A\x01\x03")[0x9B].value().isEmpty());
static_assert(parse("\xA0\x05\x9A\x01\x03\x9B\x00")[0x9B]);
static_assert(parse("\xA0\x05\x9A\x01\x03\x9B\x00")[0x9B].value().isEmpty());

// Ber length octets
static_assert(Ber<int>::lengthSize(0) == 1);
static_assert(Ber<int>::lengthSize(0x7F) == 1);
static_assert(Ber<int>::lengthSize(0x80) == 2);
static_assert(Ber<int>::lengthSize(0xFF) == 2);
static_assert(Ber<int>::lengthSize(0x100) == 3);
static_assert(Ber<int>::lengthSize(0xFFFF) == 3);
static_assert(Ber<int>::lengthSize(0x10000) == 4);
static_assert(Ber<int>::lengthSize(10 * 1024 * 1024) == 4);

// INTEGER payload is the shortest two's complement form
static_assert(Ber{0x02, 0}.payloadSize() == 1);
static_assert(Ber{0x02, 127}.payloadSize() == 1);
static_assert(Ber{0x02, 128}.payloadSize() == 2);
static_assert(Ber{0x02, -128}.payloadSize() == 1);
static_assert(Ber{0x02, -129}.payloadSize() == 2);
static_assert(Ber{0x02, 32767}.payloadSize() == 2);
static_assert(Ber{0x02, 32768}.payloadSize() == 3);
static_assert(Ber{0x02, std::numeric_limits<int>::max()}.payloadSize() == 4);
static_assert(Ber{0x02, std::numeric_limits<int>::min()}.payloadSize() == 4);

static_assert(Ber{0x02, 1}.encodedSize() == 3);
static_assert(Ber{0x02, 128}.encodedSize() == 4);
static_assert(Ber<QByteArrayView>{0x04, {}}.encodedSize() == 2);
static_assert(Ber<QByteArrayView>{0x04, QByteArrayView("cn", 2)}.encodedSize() == 4);
static_assert(Ber<Ber<int>>{0x30, {0x02, 1}}.encodedSize() == 5);
static_assert(Ber<Ber<Ber<int>>>{0x30, {0x30, {0x02, 50}}}.encodedSize() == 7);

} // namespace

class TLVTest: public QObject
{
	Q_OBJECT

private slots:
	void consumeStream();
	void encode_data();
	void encode();
	void roundTrip();
};

void TLVTest::consumeStream()
{
	// Mirrors LdapSearch::readResults: complete messages are parsed and removed,
	// an incomplete one stays in the buffer until the rest arrives
	const QByteArray first = QByteArray::fromHex("30 06 02 01 01 64 01 41");
	const QByteArray second = QByteArray::fromHex("30 81 85 02 01 01 64 81 7F") + QByteArray(0x7F, 'x');
	const QByteArray partial = QByteArray::fromHex("30 07 02 01 01 65");
	QByteArray buffer = first + second + partial;

	QList<quint32> ops;
	for(TLV message(buffer); message; message = TLV(buffer))
	{
		QCOMPARE(message.tag, 0x30U);
		TLV id(message.value());
		QCOMPARE(id.tag, 0x02U);
		TLV op = id.next();
		QVERIFY(op);
		ops.append(op.tag);
		buffer.remove(0, message.data.data() - buffer.constData() + message.length);
	}
	QCOMPARE(ops, QList<quint32>({0x64, 0x64}));
	QCOMPARE(buffer, partial);
}

template<typename T>
static void addEncodeRow(const char *name, const Ber<T> &ber, const QByteArray &expected)
{
	QTest::newRow(name) << QByteArray(ber) << ber.encodedSize() << expected;
}

void TLVTest::encode_data()
{
	QTest::addColumn<QByteArray>("encoded");
	QTest::addColumn<qsizetype>("encodedSize");
	QTest::addColumn<QByteArray>("expected");

	addEncodeRow("integer 0", Ber{0x02, 0}, QByteArray::fromHex("02 01 00"));
	addEncodeRow("integer 127", Ber{0x02, 127}, QByteArray::fromHex("02 01 7F"));
	addEncodeRow("integer 128", Ber{0x02, 128}, QByteArray::fromHex("02 02 0080"));
	addEncodeRow("integer -128", Ber{0x02, -128}, QByteArray::fromHex("02 01 80"));
	addEncodeRow("integer -129", Ber{0x02, -129}, QByteArray::fromHex("02 02 FF7F"));
	addEncodeRow("integer 32768", Ber{0x02, 32768}, QByteArray::fromHex("02 03 008000"));
	addEncodeRow("integer max", Ber{0x02, std::numeric_limits<int>::max()}, QByteArray::fromHex("02 04 7FFFFFFF"));
	addEncodeRow("integer min", Ber{0x02, std::numeric_limits<int>::min()}, QByteArray::fromHex("02 04 80000000"));
	addEncodeRow("octet string empty", Ber{0x04, QByteArray()}, QByteArray::fromHex("04 00"));
	addEncodeRow("octet string", Ber{0x04, QByteArray("c=EE")}, QByteArray::fromHex("04 04 633D4545"));
	addEncodeRow("long length 1 byte", Ber{0x04, QByteArray(0x80, 'x')},
		QByteArray::fromHex("04 81 80") + QByteArray(0x80, 'x'));
	addEncodeRow("long length 2 bytes", Ber{0x04, QByteArray(0x100, 'x')},
		QByteArray::fromHex("04 82 0100") + QByteArray(0x100, 'x'));
	// LDAP UnbindRequest as sent by LdapSearch
	addEncodeRow("unbind request", Ber{0x30, Ber{0x02, 1} + Ber{0x42, QByteArray()}},
		QByteArray::fromHex("30 05 02 01 01 42 00"));
	addEncodeRow("nested", Ber{0x30, Ber{0x30, Ber{0x02, 50}}}, QByteArray::fromHex("30 05 30 03 02 01 32"));
}

void TLVTest::encode()
{
	QFETCH(QByteArray, encoded);
	QFETCH(qsizetype, encodedSize);
	QFETCH(QByteArray, expected);

	QCOMPARE(encoded, expected);
	QCOMPARE(encoded.size(), encodedSize);

	TLV tlv(encoded);
	QVERIFY(tlv);
	QCOMPARE(tlv.tag, quint32(quint8(expected[0])));
	QCOMPARE(qsizetype(tlv.value().cend() - encoded.constData()), encoded.size());
	QVERIFY(!tlv.next());
}

void TLVTest::roundTrip()
{
	// SearchRequest substrings filter as built by LdapSearch
	const QByteArray filter = Ber{0xA4, Ber{0x04, QByteArray("serialNumber")} +
		Ber{0x30, Ber{0x81, QByteArray("PNOEE-47101010033")}}};
	TLV tlv(filter);
	QCOMPARE(tlv.tag, 0xA4U);
	QCOMPARE(tlv[0x04].value().toByteArray(), QByteArray("serialNumber"));
	QCOMPARE(tlv[0x30][0x81].value().toByteArray(), QByteArray("PNOEE-47101010033"));

	const QByteArray message = Ber{0x30, Ber{0x02, 1} + Ber{0x63, filter}};
	TLV id(TLV(message).value());
	QCOMPARE(id.tag, 0x02U);
	QCOMPARE(id.value().toByteArray(), QByteArray::fromHex("01"));
	TLV op = id.next();
	QCOMPARE(op.tag, 0x63U);
	QCOMPARE(op.value().toByteArray(), filter);
}

QTEST_APPLESS_MAIN(TLVTest)
#include "TLVTest.moc"
