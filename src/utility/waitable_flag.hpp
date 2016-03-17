#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>

class WaitableFlag
{
	mutable std::mutex _m;
	mutable std::condition_variable _cv;
	std::atomic_bool _flag;

public:
	WaitableFlag(bool set = false)
		: _flag(set)
	{}

	bool isSet() const
	{
		return _flag;
	}

	void set()
	{
		{	// Still need lock because we don't want
			// _flag to be modified between checking
			// the predicate and blocking the thread
			// in wait(), waitFor() and waitUntil().
			auto lock = makeLock();
			_flag = true;
		}
		_cv.notify_all();
	}

	void reset()
	{
		auto lock = makeLock();
		_flag = false;
	}

	void wait() const
	{
		auto lock = makeLock();
		_cv.wait(lock, [this] { return isSet(); });
	}

	template <typename Rep, typename Period>
	bool waitFor(const std::chrono::duration<Rep, Period>& relativeTime) const
	{
		auto lock = makeLock();
		return _cv.wait_for(lock, relativeTime, [this] { return isSet(); });
	}

	template <typename Clock, typename Duration>
	bool waitUntil(const std::chrono::time_point<Clock, Duration>& timeoutTime) const
	{
		auto lock = makeLock();
		return _cv.wait_until(lock, timeoutTime, [this] { return isSet(); });
	}

private:
	std::unique_lock<std::mutex> makeLock() const
	{
		return std::unique_lock<std::mutex>(_m);
	}
};
