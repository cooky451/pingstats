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

#include <chrono>
#include <type_traits>

namespace utility // export
{
	using seconds_f32 = std::chrono::duration<float, std::ratio<1, 1>>;
	using seconds_f64 = std::chrono::duration<double, std::ratio<1, 1>>;
	using milliseconds_f32 = std::chrono::duration<float, std::milli>;
	using milliseconds_f64 = std::chrono::duration<double, std::milli>;

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

		template <typename Duration>
		auto elapsed() const
		{
			return std::chrono::duration_cast<Duration>(now() - _lastTick);
		}

		auto elapsed() const
		{
			return elapsed<duration>();
		}

		template <typename Duration>
		Duration tick(time_point* tickTimePoint = nullptr)
		{
			time_point dummy;

			if (tickTimePoint == nullptr)
			{
				tickTimePoint = &dummy;
			}

			*tickTimePoint = now();

			const auto elapsedTime = *tickTimePoint - _lastTick;

			_lastTick = *tickTimePoint;

			return elapsedTime;
		}

		duration tick(time_point* tickTimePoint = nullptr)
		{
			return tick<duration>(tickTimePoint);
		}
	};

	namespace literals
	{
		using namespace std::literals;

		constexpr auto operator "" _sf32(long double x)
		{
			return seconds_f32{ static_cast<float>(x) };
		}

		constexpr auto operator "" _sf64(long double x)
		{
			return seconds_f64{ static_cast<double>(x) };
		}

		constexpr auto operator "" _msf32(long double x)
		{
			return milliseconds_f32{ static_cast<float>(x) };
		}

		constexpr auto operator "" _msf64(long double x)
		{
			return milliseconds_f64{ static_cast<double>(x) };
		}
	}
}
