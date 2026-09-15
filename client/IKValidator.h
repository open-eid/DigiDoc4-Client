// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QtGui/QValidator>

class IKValidator
{
public:
	static QDate birthDate(QStringView ik);
	static bool isValid(QStringView ik);
};



class NumberValidator final: public QValidator
{
	Q_OBJECT
public:
	explicit NumberValidator(QObject *parent = nullptr);

	State validate(QString &input, int &pos) const final;
};
