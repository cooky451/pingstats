/* 
 * Copyright (c) 2016 - 2017 cooky451
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 */

#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace utility // export
{
	class WaitableFlag
	{
		mutable std::mutex _mutex;
		mutable std::condition_variable _conditionVariable;
		std::atomic_bool _flag; // atomic so we don't need to lock for isSet()

		// set() and reset() still need to lock because
		// we don't want _flag to be modified between
		// checking the predicate and blocking the thread
		// in wait(), waitFor() and waitUntil().

	public:
		WaitableFlag(bool set = false)
			: _flag(set)
		{}

		auto isSet() const
		{
			return _flag.load();
		}

		void set()
		{
			{	
				auto lock = makeLock();
				_flag = true;
			}

			_conditionVariable.notify_all();
		}

		void reset()
		{
			auto lock = makeLock();
			_flag = false;
		}

		void wait() const
		{
			auto lock = makeLock();
			_conditionVariable.wait(lock, [this] { return isSet(); });
		}

		template <typename Rep, typename Period>
		bool waitFor(const std::chrono::duration<Rep, Period>& relativeTime) const
		{
			auto lock = makeLock();
			return _conditionVariable.wait_for(
				lock, relativeTime, [this] { return isSet(); });
		}

		template <typename Clock, typename Duration>
		bool waitUntil(const std::chrono::time_point<Clock, Duration>& timeoutTime) const
		{
			auto lock = makeLock();
			return _conditionVariable.wait_until(
				lock, timeoutTime, [this] { return isSet(); });
		}

	private:
		std::unique_lock<std::mutex> makeLock() const
		{
			return std::unique_lock<std::mutex>(_mutex);
		}
	};
}
