#pragma once

#include <condition_variable>
#include <mutex>
#include <deque>

template <typename ValueType>
class ConcurrentQueue
{
public:
	typedef ValueType value_type;

private:
	mutable std::mutex _mutex;
	mutable std::condition_variable _conditionVariable;
	std::deque<ValueType> _queue;

public:
	std::size_t size() const
	{
		std::lock_guard<std::mutex> lock(_mutex);

		return _queue.size();
	}

	void clear()
	{
		std::unique_lock<std::mutex> lock(_mutex);

		_queue.clear();
	}

	void push(const ValueType& value)
	{
		std::lock_guard<std::mutex> lock(_mutex);

		_queue.push_front(value);
		_conditionVariable.notify_one();
	}

	void push(ValueType&& value)
	{
		std::lock_guard<std::mutex> lock(_mutex);

		_queue.push_front(std::move(value));
		_conditionVariable.notify_one();
	}

	ValueType pop()
	{
		std::unique_lock<std::mutex> lock(_mutex);

		_conditionVariable.wait(lock, [this] { return !_queue.empty();  });

		auto value = std::move(_queue.back());
		_queue.pop_back();

		return value;
	}

	bool tryPop(ValueType& var)
	{
		std::lock_guard<std::mutex> lock(_mutex);

		if (!_queue.empty())
		{
			var = std::move(_queue.back());
			_queue.pop_back();
			return true;
		}

		return false;
	}

	template <typename Rep, typename Period>
	bool tryPopFor(ValueType& var, const std::chrono::duration<Rep, Period>& relativeTime)
	{
		std::unique_lock<std::mutex> lock(_mutex);

		if (_conditionVariable.wait_for(lock, relativeTime, [this] { return !_queue.empty(); }))
		{
			var = std::move(_queue.back());
			_queue.pop_back();
			return true;
		}

		return false;
	}
};
