#pragma once

#include <chrono>
#include <type_traits>

template <typename Clock = std::chrono::steady_clock>
class Stopwatch
{
public:
	typedef typename Clock::time_point time_point;
	typedef typename Clock::duration duration;

private:
	time_point _lastTick;

public:
	Stopwatch()
		: _lastTick(Clock::now())
	{}

	void reset()
	{
		*this = Stopwatch();
	}

	time_point now() const
	{
		return Clock::now();
	}

	duration elapsed() const
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
		auto elapsedTime = tickTimePoint - _lastTick;
		_lastTick = tickTimePoint;
		return elapsedTime;
	}
};

template <typename T, typename Rep, typename Period, 
	typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
T floatCast(const std::chrono::duration<Rep, Period>& duration, T mul = T(1))
{
	return duration.count() * mul * (T(Period::num) / T(Period::den));
}
