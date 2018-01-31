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

#include <QObject>

class QCardLockPrivate;

class QCardLock
{
public:
	~QCardLock();
	static QCardLock& instance();

	void exclusiveLock();
	bool exclusiveTryLock();
	void exclusiveUnlock();
	void readLock();
	bool readTryLock();
	void readUnlock();

private:
	QCardLock();
	Q_DISABLE_COPY(QCardLock);
	QCardLockPrivate *d;
};

class QCardLocker
{
public:
	inline QCardLocker()
	{ 
		QCardLock::instance().exclusiveLock();
	}
	inline ~QCardLocker()
	{
		QCardLock::instance().exclusiveUnlock();
	}

private:
	Q_DISABLE_COPY(QCardLocker)
};
