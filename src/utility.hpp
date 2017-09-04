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

#include "utility/utility.hpp"

namespace pingstats // export
{
	template <typename To, typename From>
	constexpr auto fastround(From value)
	{
		// This function doesn't handle all FP corner cases correctly, 
		// but it is about 20 times faster than std::(l)lround.
		// Absolutely needed for software rendering functions.

		static_assert(std::is_floating_point_v<From>);
		static_assert(std::is_integral_v<To>);

		if constexpr (std::is_signed_v<To>)
		{
			if (value <= From{ 0.0 })
			{
				return static_cast<To>(value - From{ 0.5 });
			}
		}
		
		return static_cast<To>(value + From{ 0.5 });
	}

	std::vector<std::string> parseWords(const std::string& source)
	{
		const auto skipSpaces{ [](const char* s) {
			for (; std::isspace(*s); ++s);
			return s;
		} };

		const auto skipNonSpaces{ [](const char* s) {
			for (; *s != '\0' && !std::isspace(*s); ++s);
			return s;
		} };

		std::vector<std::string> words;

		for (auto begin{ source.c_str() };; )
		{
			begin = skipSpaces(begin);

			const auto end{ skipNonSpaces(begin) };

			if (begin != end)
			{
				words.emplace_back(begin, end);
				begin = end;
			}
			else
			{
				break;
			}
		}

		return words;
	}

	std::string insertCarriageReturns(const std::string& str)
	{
		std::string ret(str.size() + std::count(str.begin(), str.end(), '\n'), char{});

		for (std::size_t i{}, j{}; i < str.size(); ++i, ++j)
		{
			if (str[i] == '\n')
			{
				ret[j++] = '\r';
			}

			ret[j] = str[i];
		}

		return ret;
	}
}

