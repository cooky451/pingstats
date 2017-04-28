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
			return _conditionVariable.wait_for(lock, relativeTime, [this] { return isSet(); });
		}

		template <typename Clock, typename Duration>
		bool waitUntil(const std::chrono::time_point<Clock, Duration>& timeoutTime) const
		{
			auto lock = makeLock();
			return _conditionVariable.wait_until(lock, timeoutTime, [this] { return isSet(); });
		}

	private:
		std::unique_lock<std::mutex> makeLock() const
		{
			return std::unique_lock<std::mutex>(_mutex);
		}
	};
}
