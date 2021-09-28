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

#include <QEventLoop>
#include <exception>
#include <thread>

namespace {
	template <typename F>
	inline void waitFor(F&& function) {
		std::exception_ptr exception;
		QEventLoop l;
		std::thread([&]{
			try {
				function();
			} catch(...) {
				exception = std::current_exception();
			}
			l.exit();
		}).detach();
		l.exec();
		if(exception)
			std::rethrow_exception(exception);
	}

	inline QString escapeUnicode(const QString &str) {
		QString escaped;
		escaped.reserve(6 * str.size());
		for (QChar ch: str) {
			ushort code = ch.unicode();
			if (code < 0x80) {
				escaped += ch;
			} else {
				escaped += QString("\\u");
				escaped += QString::number(code, 16).rightJustified(4, '0');
			}
		}
		return escaped;
	}
}
