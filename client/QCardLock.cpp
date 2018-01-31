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

#include "QCardLock.h"
#include "QCardLock_p.h"


QCardLockPrivate::QCardLockPrivate()
: readLock(QMutex::Recursive)
, exclusiveLock(QMutex::NonRecursive)
{}

QCardLock::QCardLock()
: d(new QCardLockPrivate)
{
}

QCardLock::~QCardLock()
{
	delete d;
}

void QCardLock::exclusiveLock()
{
	readLock();
	d->exclusiveLock.lock();
}

bool QCardLock::exclusiveTryLock()
{
	readLock();
	bool rc = d->exclusiveLock.tryLock();
	if(!rc)
		readUnlock();
	return rc;
}

void QCardLock::exclusiveUnlock()
{
	d->exclusiveLock.unlock();
	readUnlock();
}

QCardLock& QCardLock::instance()
{
	static QCardLock lock;
	return lock;
}

void QCardLock::readLock()
{
	d->readLock.lock();
}

bool QCardLock::readTryLock()
{
	return d->readLock.tryLock();
}

void QCardLock::readUnlock()
{
	d->readLock.unlock();
}
