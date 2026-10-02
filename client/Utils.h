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
#include <QIODevice>
#include <QTimer>

#include <exception>
#include <future>
#include <limits>
#include <memory>
#include <thread>

namespace {
	template<auto D>
	struct free_deleter
	{
		template<class T>
		void operator()(T *p) const noexcept
		{
			D(p);
		}
	};

	template<auto F, typename T>
	[[nodiscard]]
	constexpr auto make_unique_ptr(T *t) noexcept
	{
		return std::unique_ptr<T, free_deleter<F>>(t);
	}

	template <typename F, class... Args>
	inline auto waitFor(F&& function, Args&& ...args) {
		QEventLoop l;
		using result_t = std::invoke_result_t<F, Args...>;
		std::packaged_task<result_t()> task(
			[function = std::forward<F>(function),
			...args = std::forward<Args>(args)]() mutable -> result_t {
				return std::invoke(function, args...);
			});
		auto future = task.get_future();
		std::jthread worker([&l, task = std::move(task)]() mutable {
			task();
			QMetaObject::invokeMethod(&l, &QEventLoop::quit, Qt::QueuedConnection);
		});
		l.exec();
		return future.get();
	}

	template <typename Sender, typename Signal>
	inline void waitForSignal(Sender *sender, Signal signal) {
		QEventLoop l;
		QObject::connect(sender, signal, &l, &QEventLoop::quit);
		l.exec();
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

	inline qint64 copyIODevice(QIODevice *from, QIODevice *to, qint64 max = (std::numeric_limits<qint64>::max)())
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
