#pragma once

#include <chrono>
#include <type_traits>

namespace utility // export
{
	template <typename Clock = std::chrono::steady_clock>
	class Stopwatch
	{
	public:
		using time_point = typename Clock::time_point;
		using duration = typename Clock::duration;

	private:
		time_point _lastTick;

	public:
		Stopwatch()
			: _lastTick(now())
		{}

		void reset()
		{
			*this = Stopwatch();
		}

		auto now() const
		{
			return Clock::now();
		}

		auto elapsed() const
		{
			return now() - _lastTick;
		}

		duration tick()
		{
			time_point dummy;
			return tick(dummy);
		}

		duration tick(time_point& tickTimePoint)
		{
			tickTimePoint = now();
			const auto elapsedTime = tickTimePoint - _lastTick;
			_lastTick = tickTimePoint;
			return elapsedTime;
		}
	};
}
