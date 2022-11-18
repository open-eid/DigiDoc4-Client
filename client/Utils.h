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
#include <QTimer>
#include <exception>
#include <thread>

namespace {
	template <typename F, class... Args>
	inline auto waitFor(F&& function, Args&& ...args) {
		std::exception_ptr exception;
		std::invoke_result_t<F,Args...> result{};
		QEventLoop l;
		// c++20 ... args == std::forward<Args>(args)
		std::thread([&, function = std::forward<F>(function)]{
			try {
				result = std::invoke(function, args...);
			} catch(...) {
				exception = std::current_exception();
			}
			l.exit();
		}).detach();
		l.exec();
		if(exception)
			std::rethrow_exception(exception);
		return result;
	}

	template <typename F, class... Args>
	inline auto dispatchToMain(F&& function, Args&& ...args) {
		std::invoke_result_t<F,Args...> result{};
		QEventLoop l;
		QTimer* timer = new QTimer();
		timer->moveToThread(qApp->thread());
		timer->setSingleShot(true);
		QObject::connect(timer, &QTimer::timeout, timer, [&, function = std::forward<F>(function)] {
			result = std::invoke(function, args...);
			l.exit();
			timer->deleteLater();
		});
		QMetaObject::invokeMethod(timer, "start", Qt::QueuedConnection, Q_ARG(int, 0));
		l.exec();
		return result;
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
