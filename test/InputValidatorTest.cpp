// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#include "IKValidator.h"

#include <QtCore/QDate>
#include <QtTest/QTest>
#include <QtWidgets/QLineEdit>

class InputValidatorTest: public QObject
{
	Q_OBJECT

private slots:
	void numberValidator_data();
	void numberValidator();
	void numberValidatorLineEdit_data();
	void numberValidatorLineEdit();
	void numberValidatorCursor();
	void personalCode_data();
	void personalCode();
	void birthDate_data();
	void birthDate();
};

void InputValidatorTest::numberValidator_data()
{
	QTest::addColumn<QString>("input");
	QTest::addColumn<QString>("expected");

	QTest::newRow("digits") << QStringLiteral("38002240211") << QStringLiteral("38002240211");
	QTest::newRow("empty") << QString() << QString();
	QTest::newRow("letters only") << QStringLiteral("abc") << QString();
	QTest::newRow("control character before") << QStringLiteral("\x1b" "38002240211") << QStringLiteral("38002240211");
	QTest::newRow("newline after") << QStringLiteral("38002240211\n") << QStringLiteral("38002240211");
	// Personal code or phone number copied with separators: every digit must stay
	QTest::newRow("personal code with spaces") << QStringLiteral("3800 2240 211") << QStringLiteral("38002240211");
	QTest::newRow("phone with plus and spaces") << QStringLiteral("+372 5555 5555") << QStringLiteral("37255555555");
	QTest::newRow("phone with dashes") << QStringLiteral("372-5555-5555") << QStringLiteral("37255555555");
	QTest::newRow("control character inside") << QStringLiteral("3800\x01" "2240211") << QStringLiteral("38002240211");
	QTest::newRow("NBSP inside") << QStringLiteral("3800 2240211") << QStringLiteral("38002240211");
	// Only ASCII digits are sent to the services
	QTest::newRow("full-width digits") << QStringLiteral("３８００") << QString();
	QTest::newRow("Arabic-Indic digits") << QStringLiteral("٣٨٠٠") << QString();
}

void InputValidatorTest::numberValidator()
{
	QFETCH(QString, input);
	QFETCH(QString, expected);
	NumberValidator validator;
	int pos = int(input.size());
	QCOMPARE(validator.validate(input, pos), QValidator::Acceptable);
	QCOMPARE(input, expected);
	QCOMPARE(pos, expected.size());
}

void InputValidatorTest::numberValidatorLineEdit_data()
{
	numberValidator_data();
}

void InputValidatorTest::numberValidatorLineEdit()
{
	// Typed or pasted into the field
	QFETCH(QString, input);
	QFETCH(QString, expected);
	QLineEdit edit;
	edit.setValidator(new NumberValidator(&edit));
	edit.insert(input);
	QCOMPARE(edit.text(), expected);
	QCOMPARE(edit.cursorPosition(), expected.size());
}

void InputValidatorTest::numberValidatorCursor()
{
	QLineEdit edit;
	edit.setValidator(new NumberValidator(&edit));
	edit.setText(QStringLiteral("3800"));
	edit.setCursorPosition(2);
	edit.insert(QStringLiteral(" 5-"));
	QCOMPARE(edit.text(), QStringLiteral("38500"));
	QCOMPARE(edit.cursorPosition(), 3);
}

void InputValidatorTest::personalCode_data()
{
	QTest::addColumn<QString>("code");
	QTest::addColumn<bool>("valid");

	QTest::newRow("Mobile-ID test code") << QStringLiteral("38002240211") << true;
	QTest::newRow("Smart-ID test code") << QStringLiteral("30303039914") << true;
	QTest::newRow("checksum 0") << QStringLiteral("38002240210") << true;
	QTest::newRow("wrong checksum") << QStringLiteral("38002240212") << false;
	QTest::newRow("10 digits") << QStringLiteral("3800224021") << false;
	QTest::newRow("12 digits") << QStringLiteral("380022402111") << false;
	QTest::newRow("leading space") << QStringLiteral(" 3800224021") << false;
	QTest::newRow("invalid date") << QStringLiteral("38013240211") << false;
	QTest::newRow("century digit 0") << QStringLiteral("08002240211") << false;
	QTest::newRow("full-width digits") << QStringLiteral("３８００２２４０２１１") << false;
	// toInt() returns 0 for a non-digit, so these passed a checksum of 0
	QTest::newRow("letter as check character") << QStringLiteral("3800224021a") << false;
	QTest::newRow("control character as check character") << QStringLiteral("3800224021\x01") << false;
	QTest::newRow("letter inside") << QStringLiteral("3800224a210") << false;
}

void InputValidatorTest::personalCode()
{
	QFETCH(QString, code);
	QFETCH(bool, valid);
	QCOMPARE(IKValidator::isValid(code), valid);
}

void InputValidatorTest::birthDate_data()
{
	QTest::addColumn<QString>("code");
	QTest::addColumn<QDate>("date");

	QTest::newRow("1800s") << QStringLiteral("18001010000") << QDate(1880, 1, 1);
	QTest::newRow("1900s") << QStringLiteral("38002240211") << QDate(1980, 2, 24);
	QTest::newRow("2000s") << QStringLiteral("60001019906") << QDate(2000, 1, 1);
	QTest::newRow("2100s") << QStringLiteral("70001010000") << QDate(2100, 1, 1);
	QTest::newRow("leap day") << QStringLiteral("50002290000") << QDate(2000, 2, 29);
	QTest::newRow("no leap day") << QStringLiteral("39002290000") << QDate();
	QTest::newRow("century digit 9") << QStringLiteral("98002240211") << QDate();
	QTest::newRow("too short") << QStringLiteral("3800224021") << QDate();
}

void InputValidatorTest::birthDate()
{
	QFETCH(QString, code);
	QFETCH(QDate, date);
	QCOMPARE(IKValidator::birthDate(code), date);
}

QTEST_MAIN(InputValidatorTest)
#include "InputValidatorTest.moc"
