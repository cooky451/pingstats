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

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>

#include <chrono>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

namespace utility // export
{
	template <std::size_t N, typename... Args>
	std::string formatString(const char(&format)[N], Args&&... args)
	{
		const auto size = std::snprintf(nullptr, 0, format, args...);

		if (size < 0)
		{
			throw std::logic_error("Invalid format string.");
		}

		// legal since LWG 2475
		std::string result(static_cast<std::size_t>(size), {});
		std::snprintf(result.data(), 1 + result.size(), format, args...);
		return result;
	}

	namespace literals
	{
		using namespace std::literals;

		constexpr auto operator "" _i8(unsigned long long int x)
		{
			return static_cast<std::int8_t>(x);
		}

		constexpr auto operator "" _u8(unsigned long long int x)
		{
			return static_cast<std::uint8_t>(x);
		}

		constexpr auto operator "" _i16(unsigned long long int x)
		{
			return static_cast<std::int16_t>(x);
		}

		constexpr auto operator "" _u16(unsigned long long int x)
		{
			return static_cast<std::uint16_t>(x);
		}

		constexpr auto operator "" _i32(unsigned long long int x)
		{
			return static_cast<std::int32_t>(x);
		}

		constexpr auto operator "" _u32(unsigned long long int x)
		{
			return static_cast<std::uint32_t>(x);
		}

		constexpr auto operator "" _i64(unsigned long long int x)
		{
			return static_cast<std::int64_t>(x);
		}

		constexpr auto operator "" _u64(unsigned long long int x)
		{
			return static_cast<std::uint64_t>(x);
		}

		constexpr auto operator "" _int(unsigned long long int x)
		{
			return static_cast<std::intptr_t>(x);
		}

		constexpr auto operator "" _unt(unsigned long long int x)
		{
			return static_cast<std::uintptr_t>(x);
		}

		constexpr auto operator "" _pdt(unsigned long long int x)
		{
			return static_cast<std::ptrdiff_t>(x);
		}

		constexpr auto operator "" _szt(unsigned long long int x)
		{
			return static_cast<std::size_t>(x);
		}
	}
}
