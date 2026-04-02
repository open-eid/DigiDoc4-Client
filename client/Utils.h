// SPDX-FileCopyrightText: Estonian Information System Authority
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QEventLoop>
#include <QIODevice>
#include <QTimer>
#include <exception>
#include <thread>

namespace {
	template <typename F, class... Args>
	inline auto waitFor(F&& function, Args&& ...args) {
		std::exception_ptr exception;
		std::invoke_result_t<F,Args...> result{};
		QEventLoop l;
		std::thread([&, function = std::forward<F>(function), ...args = std::forward<Args>(args)]{
			try {
				result = std::invoke(function, args...);
			} catch(...) {
				exception = std::current_exception();
			}
			l.exit();
		}).detach();
		l.exec();
		if(exception)
			std::rethrow_exception(std::move(exception));
		return result;
	}

	template <typename F, class... Args>
	inline auto dispatchToMain(F&& function, Args&& ...args) {
		QEventLoop l;
		using result_t = typename std::invoke_result_t<F,Args...>;
		if constexpr (std::is_void_v<result_t>) {
			QMetaObject::invokeMethod(qApp, [&, function = std::forward<F>(function), ...args = std::forward<Args>(args)] {
				std::invoke(function, args...);
				l.exit();
			}, Qt::QueuedConnection);
			l.exec();
		} else {
			result_t result{};
			QMetaObject::invokeMethod(qApp, [&, function = std::forward<F>(function), ...args = std::forward<Args>(args)] {
				result = std::invoke(function, args...);
				l.exit();
			}, Qt::QueuedConnection);
			l.exec();
			return result;
		}
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

	inline qint64 copyIODevice(QIODevice *from, QIODevice *to, qint64 max = std::numeric_limits<qint64>::max())
	{
		std::array<char,16*1024> buf{};
		qint64 size = 0, i = 0;
		for(; (i = from->read(buf.data(), std::min<qint64>(max, buf.size()))) > 0; size += i, max -= i)
		{
			if(to->write(buf.data(), i) != i)
				return -1;
		}
		return i < 0 ? i : size;
	}
}
